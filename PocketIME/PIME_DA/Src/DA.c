#include <PalmOS.h>
#include <common\system\HsKeyCodes.h>
#include <core\system\SysEvent.h>
#define appName						"PocketIME"
#define sysAppLaunchCmdDALaunch			60000

void da_main(void)
{
	  LocalID  dbID= DmFindDatabase(0, appName);
	  UInt32  result;
	  if (dbID)
	  {
	    SysAppLaunch(0, dbID, 0, sysAppLaunchCmdDALaunch, NULL, &result);//60000
	    
	  }
	  //EvtEnqueueKey(hsKeySymbol,0,optionKeyMask | virtualKeyMask);
}