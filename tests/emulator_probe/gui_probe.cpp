// Test executable only. Uses the existing Qt emulator source unchanged, with
// an ordinary button-click test on a namespace-private virtual CAN interface.
#define main existing_emulator_main
#include EMULATOR_MAIN_SOURCE
#undef main
#include <cstdlib>
int main(int argc,char** argv){
    if(argc!=3||std::string(argv[1])!="rnettest0"||qEnvironmentVariable("RNET_ISOLATED_EMULATOR_TEST")!="1")return 2;
    QApplication app(argc,argv);MainWindow window(QString::fromLocal8Bit(argv[1]),QString::fromLocal8Bit(argv[2]));
    window.setProperty("rnetRunMode",QStringLiteral("emu"));window.show();
    QTimer::singleShot(300,&window,[&]{
        QPushButton* control=nullptr;
        for(auto* button:window.findChildren<QPushButton*>())if(button->text()==QStringLiteral("EXTERN EIN / AUS")){if(control){app.exit(3);return;}control=button;}
        if(!control||!control->isEnabled()){app.exit(4);return;}
        control->click();std::fprintf(stdout,"ISOLATED_TEST: clicked emulator power button on rnettest0 only\n");std::fflush(stdout);
    });
    QTimer snapshot;QObject::connect(&snapshot,&QTimer::timeout,&window,[&]{
        auto* text=window.findChild<QPlainTextEdit*>();if(!text)return;
        QFile file(QStringLiteral("emulator-ui-state.txt"));if(file.open(QIODevice::WriteOnly|QIODevice::Truncate))file.write(text->toPlainText().toUtf8());
    });snapshot.start(500);
    QTimer::singleShot(150000,&app,&QApplication::quit);
    return app.exec();
}
