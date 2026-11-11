// Read-only reverse-engineering evidence; never part of the product.
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;
public class OemAccessEvidence extends GhidraScript {
    public void run() throws Exception {
        String[] args=getScriptArgs(); File dir=new File(args[0]);dir.mkdirs();
        DecompInterface di=new DecompInterface();di.openProgram(currentProgram);
        LinkedHashSet<Function> focus=new LinkedHashSet<>();
        for(int i=1;i<args.length;i++) {
            Function f=getFunctionContaining(toAddr(args[i]));if(f!=null)focus.add(f);
        }
        try(PrintWriter refs=new PrintWriter(new File(dir,"callers.tsv"))) {
            for(int depth=0;depth<3;depth++) {
                for(Function f:new ArrayList<>(focus)) {
                    ReferenceIterator ri=currentProgram.getReferenceManager().getReferencesTo(f.getEntryPoint());
                    while(ri.hasNext()) {
                        Reference r=ri.next(); Function caller=getFunctionContaining(r.getFromAddress());
                        refs.println(f.getEntryPoint()+"\t"+r.getFromAddress()+"\t"+(caller==null?"data":caller.getEntryPoint()));
                        if(caller!=null&&!caller.isExternal()&&!caller.isThunk())focus.add(caller);
                    }
                }
            }
            for(Function f:new ArrayList<>(focus))for(Function c:f.getCalledFunctions(monitor))
                if(!c.isExternal()&&!c.isThunk())focus.add(c);
            for(Function f:focus) {
                DecompileResults r=di.decompileFunction(f,30,monitor);
                try(PrintWriter w=new PrintWriter(new File(dir,f.getEntryPoint()+".c"))) {
                    w.println("/* Evidence only: "+f.getName(true)+"; original SHA256 "+currentProgram.getExecutableSHA256()+" */");
                    w.println(r.decompileCompleted()?r.getDecompiledFunction().getC():r.getErrorMessage());
                }
            }
        } finally {di.dispose();}
        println("Exported "+focus.size()+" functions");
    }
}
