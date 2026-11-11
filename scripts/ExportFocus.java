// Private, read-only analysis export. Never compiled into the application.
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;
public class ExportFocus extends GhidraScript {
    public void run() throws Exception {
        println("Executable SHA256: "+currentProgram.getExecutableSHA256());
        String[] args=getScriptArgs(); File dir=new File(args[0]);dir.mkdirs();
        DecompInterface di=new DecompInterface();di.openProgram(currentProgram);
        LinkedHashSet<Function> focus=new LinkedHashSet<>();
        try(PrintWriter symbols=new PrintWriter(new File(dir,"functions.tsv"));PrintWriter refs=new PrintWriter(new File(dir,"focus-xrefs.tsv"))){
            FunctionIterator fi=currentProgram.getFunctionManager().getFunctions(true);
            while(fi.hasNext()) {Function f=fi.next();symbols.println(f.getEntryPoint()+"\t"+f.getName(true));}
            if(args.length>1){
                for(int i=1;i<args.length;i++){Function f=getFunctionContaining(toAddr(args[i]));if(f!=null)focus.add(f);}
            } else {
                DataIterator it=currentProgram.getListing().getDefinedData(true);
                while(it.hasNext()){
                    Data d=it.next();Object val=d.getValue();if(!(val instanceof String))continue;
                    String s=(String)val;String l=s.toLowerCase();
                    if(!(l.contains("rnd")||l.contains("encrypt")||l.contains("r-net files")||l.contains("unable to create rnet")||l.contains("parameter file")||l.contains("rnet programmer oem")))continue;
                    ReferenceIterator ri=currentProgram.getReferenceManager().getReferencesTo(d.getAddress());
                    while(ri.hasNext()) {Reference r=ri.next(); Function f=getFunctionContaining(r.getFromAddress());refs.println(d.getAddress()+"\t"+s.replace('\n',' ')+"\t"+r.getFromAddress()+"\t"+(f==null?"data":f.getEntryPoint()));if(f!=null)focus.add(f);}
                }
                for(int depth=0;depth<2;depth++)for(Function f:new ArrayList<>(focus))for(Function c:f.getCalledFunctions(monitor))if(!c.isExternal()&&!c.isThunk()&&c.getEntryPoint().getOffset()<0x500000)focus.add(c);
            }
            for(int depth=0;depth<1;depth++)for(Function f:new ArrayList<>(focus))for(Function c:f.getCalledFunctions(monitor))if(!c.isExternal()&&!c.isThunk())focus.add(c);
            for(Function f:focus){
                if(monitor.isCancelled())break;
                DecompileResults r=di.decompileFunction(f,25,monitor);
                try(PrintWriter w=new PrintWriter(new File(dir,f.getEntryPoint()+".c"))){w.println("/* Evidence only; not buildable source. "+f.getName(true)+" */");if(r.decompileCompleted())w.println(r.getDecompiledFunction().getC());else w.println(r.getErrorMessage());}
            }
        }finally{di.dispose();}
        println("Exported focused functions: "+focus.size());
    }
}
