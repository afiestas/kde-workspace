#include <shutdown.h>
#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <kapplication.h>
#include <kiconloader.h>
#include <kworkspace.h>

int
main(int argc, char *argv[])
{
   KAboutData about("kapptest", "kapptest", "version");
   KCmdLineArgs::init(argc, argv, &about);

   KApplication a;
   a.iconLoader()->addAppDir("ksmserver");
   KSMShutdownFeedback::start();

   KWorkSpace::ShutdownType sdtype = KWorkSpace::ShutdownTypeNone;
   (void)KSMShutdownDlg::confirmShutdown( true,
                                          sdtype );
/*   (void)KSMShutdownDlg::confirmShutdown( false,
                                          sdtype ); */

   KSMShutdownFeedback::stop();
}
