/* $TOG: dm.h /main/67 1998/02/09 13:55:01 kaleb $ */
/* $Id$ */
/*

Copyright 1988, 1998  The Open Group

All Rights Reserved.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

*/
/* $XFree86: xc/programs/xdm/dm.h,v 3.19 2000/06/14 00:16:14 dawes Exp $ */

/*
 * xdm - display manager daemon
 * Author:  Keith Packard, MIT X Consortium
 *
 * dm.h
 *
 * public interfaces for greet/verify functionality
 */

#ifndef _DM_H_
#define _DM_H_ 1

#include "kdm-config.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef MINIX
# ifdef MNX_TCPCONN
#  define TCPCONN
# endif
#endif /* MINIX */

#include <X11/Xos.h>
#include <X11/Xfuncs.h>
#include <X11/Xmd.h>
#include <X11/Xauth.h>
#include <X11/Intrinsic.h>

#if defined(X_POSIX_C_SOURCE)
# define _POSIX_C_SOURCE X_POSIX_C_SOURCE
# include <setjmp.h>
# include <limits.h>
# undef _POSIX_C_SOURCE
#else
# if defined(X_NOT_POSIX) || defined(_POSIX_SOURCE)
#  include <setjmp.h>
#  include <limits.h>
# else
#  define _POSIX_SOURCE
#  include <setjmp.h>
#  include <limits.h>
#  undef _POSIX_SOURCE
# endif
#endif
#ifdef X_NOT_STDC_ENV
# define Time_t long
extern Time_t time ();
#else
# include <time.h>
# define Time_t time_t
#endif

#ifdef XDMCP
# include <X11/Xdmcp.h>
#endif

#ifdef pegasus
# undef dirty		/* Some bozo put a macro called dirty in sys/param.h */
#endif /* pegasus */

#ifndef X_NOT_POSIX
# ifdef _POSIX_SOURCE
#  include <sys/wait.h>
# else
#  define _POSIX_SOURCE
#  ifdef SCO325
#   include <sys/procset.h>
#   include <sys/siginfo.h>
#  endif
#  include <sys/wait.h>
#  undef _POSIX_SOURCE
# endif
# define waitCode(w)	(WIFEXITED(w) ? WEXITSTATUS(w) : 0)
# define waitSig(w)	(WIFSIGNALED(w) ? WTERMSIG(w) : 0)
# define waitCore(w)    0	/* not in POSIX.  so what? */
typedef int		waitType;
#else /* X_NOT_POSIX */
# ifdef SYSV
#  define waitCode(w)	(((w) >> 8) & 0x7f)
#  define waitSig(w)	((w) & 0xff)
#  define waitCore(w)	(((w) >> 15) & 0x01)
typedef int		waitType;
# else /* SYSV */
#  include <sys/wait.h>
#  define waitCode(w)	((w).w_T.w_Retcode)
#  define waitSig(w)	((w).w_T.w_Termsig)
#  define waitCore(w)	((w).w_T.w_Coredump)
typedef union wait	waitType;
# endif
#endif /* X_NOT_POSIX */

#ifdef USE_PAM
# include <security/pam_appl.h>
#endif


#define waitCompose(sig,core,code) ((sig) * 256 + (core) * 128 + (code))
#define waitVal(w) waitCompose(waitSig(w), waitCore(w), waitCode(w))

typedef enum displayStatus { running, notRunning, zombie, phoenix, suspended } DisplayStatus;

/* XXX uncomment this, if it's really needed!!!!!
#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>                / * Defines fd_set on some systems * /
#endif
*/

#ifndef FD_ZERO
typedef	struct	my_fd_set { int fds_bits[1]; } my_fd_set;
# define FD_ZERO(fdp)	bzero ((fdp), sizeof (*(fdp)))
# define FD_SET(f,fdp)	((fdp)->fds_bits[(f) / (sizeof (int) * 8)] |=  (1 << ((f) % (sizeof (int) * 8))))
# define FD_CLR(f,fdp)	((fdp)->fds_bits[(f) / (sizeof (int) * 8)] &= ~(1 << ((f) % (sizeof (int) * 8))))
# define FD_ISSET(f,fdp)	((fdp)->fds_bits[(f) / (sizeof (int) * 8)] & (1 << ((f) % (sizeof (int) * 8))))
# define FD_TYPE	my_fd_set
#else
# define FD_TYPE	fd_set
#endif

/*
 * local     - server runs on local host
 * foreign   - server runs on remote host
 * permanent - session restarted when it exits
 * transient - session not restarted when it exits
 * fromFile  - started via entry in servers file
 * fromXDMCP - started with XDMCP
 */

typedef struct displayType {
	unsigned int	location:1;
	unsigned int	lifetime:1;
	unsigned int	origin:1;
} DisplayType;

# define Local		1
# define Foreign	0

# define Permanent	1
# define Transient	0

# define FromFile	1
# define FromXDMCP	0

extern DisplayType parseDisplayType (char *string, int *usedDefault, char **atPos);

typedef enum fileState { NewEntry, OldEntry, MissingEntry } FileState;

struct display {
	struct display	*next;
	/* Xservers file / XDMCP information */
	char		*name;		/* DISPLAY name -- also referenced in hstent */
	char		*class2;	/* display class (may be NULL) */
	DisplayType	displayType;	/* method to handle with */
	char		**argv;		/* program name and arguments */

	/* display state */
	DisplayStatus	status;		/* current status */
	int		pid;		/* process id of child */
	int		serverPid;	/* process id of server (-1 if none) */
	FileState	state;		/* state during HUP processing */
	int		startTries;	/* current start try -- only for binary compatibility */
#ifdef XDMCP
	/* XDMCP state */
	CARD32		sessionID;	/* ID of active session */
	XdmcpNetaddr    peer;		/* display peer address */
	int		peerlen;	/* length of peer address */
	XdmcpNetaddr    from;		/* XDMCP port of display */
	int		fromlen;
	CARD16		displayNumber;
	int		useChooser;	/* Run the chooser for this display */
	ARRAY8		clientAddr;	/* for chooser picking */
	CARD16		connectionType;	/* ... */
#endif
	/* server management resources */
	int		serverAttempts;	/* number of attempts at running X */
	int		openDelay;	/* server{Timeout,Delay} fit better */
	int		openRepeat;	/* connection open attempts to make */
	int		openTimeout;	/* abort open attempt timeout */
	int		startAttempts;	/* number of attempts at starting */
	int		pingInterval;	/* interval between XSync */
	int		pingTimeout;	/* timeout for XSync */
	int		terminateServer;/* restart for each session */
	int		grabServer;	/* keep server grabbed for Login */
	int		grabTimeout;	/* time to wait for grab */
	int		resetSignal;	/* signal to reset server */
	int		termSignal;	/* signal to terminate server */
	int		resetForAuth;	/* server reads auth file at reset */
	char            *keymaps;       /* binary compat with DEC */
	char		*greeterLib;	/* greeter shared library name */

	/* session resources */
	char		*resources;	/* resource file */
	char		*xrdb;		/* xrdb program */
	char		*setup;		/* Xsetup program */
	char		*startup;	/* Xstartup program */
	char		*reset;		/* Xreset program */
	char		*session;	/* Xsession program */
	char		*userPath;	/* path set for session */
	char		*systemPath;	/* path set for startup/reset */
	char		*systemShell;	/* interpreter for startup/reset */
	char		*failsafeClient;/* a client to start when the session fails */
	char		*chooser;	/* chooser program */

	/* authorization resources */
	int		authorize;	/* enable authorization */
	char		**authNames;	/* authorization protocol names */
	unsigned short	*authNameLens;	/* authorization protocol name lens */
	char		*clientAuthFile;/* client specified auth file */
	char		*userAuthDir;	/* backup directory for tickets */
	int		authComplain;	/* complain when no auth for XDMCP */

	/* information potentially derived from resources */
	int		authNameNum;	/* number of protocol names */
	Xauth		**authorizations;/* authorization data */
	int		authNum;	/* number of authorizations */
	char		*authFile;	/* file to store authorization in */

	int		version;	/* to keep dynamic greeter clued in */
	/* add new fields only after here.  And preferably at the end. */

	/* Hack for making "Willing to manage" configurable */
	char		*willing;	/* "Willing to manage" program */

	/* belongs to session management resources */
	int		autoLogin;	/* enable. need this herein for dynamic greeter */
	int		autoReLogin;	/* auto-re-login after crash */
	int		autoLogin1st;	/* auto-login at startup? */
	char		*autoUser;	/* user to log in automatically. */
	char		*autoPass;	/* his password. only for krb5 & sec_rpc */
	char		*autoString;	/* xsession arguments. */
	char		*noPassUsers;	/* users allowed in without a password */

	int		allowNullPasswd;/* allow null password on login */
	int		allowRootLogin;	/* allow direct root login */

	/* belongs to server management resources */
	int		fifoCreate;	/* create a login data fifo */
	int		fifoOwner;	/* owner of fifo */
	int		fifoGroup;	/* group of fifo */
	char		*console;	/* the tty line hidden by the server */

	/* misc server state */
	int		startInterval;	/* reset startAttempts after this time */
	int		fifofd;		/* fifo for login after logout */
	int		pipefd[2];	/* pipe for re-login after crash */

	struct disphist	*hstent;	/* display history entry */
};

struct disphist {
	struct disphist	*next;
	char		*name;
	Time_t		lastExit;	/* time of last display exit */
	int		goodExit;	/* was the last exit "peaceful"? */
	int		startTries;	/* current start try */
	char		*nLogPipe;	/* data read from fifo */
};

#ifdef XDMCP

#define PROTO_TIMEOUT	(30 * 60)   /* 30 minutes should be long enough */

struct protoDisplay {
	struct protoDisplay	*next;
	XdmcpNetaddr		address;   /* UDP address */
	int			addrlen;    /* UDP address length */
	unsigned long		date;	    /* creation date */
	CARD16			displayNumber;
	CARD16			connectionType;
	ARRAY8			connectionAddress;
	CARD32			sessionID;
	Xauth			*fileAuthorization;
	Xauth			*xdmcpAuthorization;
	ARRAY8			authenticationName;
	ARRAY8			authenticationData;
	XdmAuthKeyRec		key;
};
#endif /* XDMCP */

struct greet_info {
	char		*name;		/* user name */
	char		*password;	/* user password */
	char		*string;	/* random string */
	char            *passwd;        /* binary compat with DEC */
	int		version;	/* for dynamic greeter to see */
	/* add new fields below this line, and preferably at the end */
};

/* setgroups is not covered by POSIX, arg type varies */
#if defined(SYSV) || defined(SVR4) || defined(__osf__) || defined(linux) || defined(__GNU__)
# define GID_T gid_t
#else
# define GID_T int
#endif

struct verify_info {
	int		uid;		/* user id */
	int		gid;		/* group id */
	char		**argv;		/* arguments to session */
	char		**userEnviron;	/* environment for session */
	char		**systemEnviron;/* environment for startup/reset */
	int		version;	/* for dynamic greeter to see */
	/* add new fields below this line, and preferably at the end */
};

/* display manager exit status definitions */

# define OBEYSESS_DISPLAY	0	/* obey multipleSessions resource */
# define REMANAGE_DISPLAY	1	/* force remanage */
# define UNMANAGE_DISPLAY	2	/* force deletion */
# define RESERVER_DISPLAY	3	/* force server termination */
# define OPENFAILED_DISPLAY	4	/* XOpenDisplay failed, retry */
# define RESERVER_AL_DISPLAY	5	/* reserver; maybe, auto-(re-)login */
# define ALTMODE_DISPLAY	6	/* start console login */

#ifndef TRUE
# define TRUE	1
# define FALSE	0
#endif

extern char	*config;
extern char	*config2Parse;

extern char	*servers;
extern int	request_port;
extern int	debugLevel;
extern char	*errorLogFile;
extern int	daemonMode;
extern char	*pidFile;
extern int	lockPidFile;
extern char	*authDir;
extern int	autoRescan;
extern int	removeDomainname;
extern char	*keyFile;
extern char	*accessFile;
extern char	**exportList;
extern char	*randomFile;
extern char	*greeterLib;
extern char	*willing;
extern int	choiceTimeout;	/* chooser choice timeout */
extern int	autoLogin;

extern struct display
	*FindDisplayByName (char *name),
#ifdef XDMCP
	*FindDisplayBySessionID (CARD32 sessionID),
	*FindDisplayByAddress (XdmcpNetaddr addr, int addrlen, CARD16 displayNumber),
#endif /* XDMCP */
	*FindDisplayByPid (int pid),
	*FindDisplayByServerPid (int serverPid),
	*NewDisplay (char *name, char *class2);

#ifdef XDMCP
extern struct protoDisplay	*FindProtoDisplay (
					XdmcpNetaddr address,
					int          addrlen,
					CARD16       displayNumber);
extern struct protoDisplay	*NewProtoDisplay (
					XdmcpNetaddr address,
					int	     addrlen,
					CARD16	     displayNumber,
					CARD16	     connectionType,
					ARRAY8Ptr    connectionAddress,
					CARD32	     sessionID);

typedef void (*ChooserFunc)(CARD16 connectionType, ARRAY8Ptr addr, char *closure);

/* in access.c */
extern ARRAY8Ptr getLocalAddress (void);
extern int AcceptableDisplayAddress (ARRAY8Ptr clientAddress, CARD16 connectionType, xdmOpCode type);
extern int ForEachMatchingIndirectHost (ARRAY8Ptr clientAddress, CARD16 connectionType, ChooserFunc function, char *closure);
extern int ScanAccessDatabase (void);
extern int UseChooser (ARRAY8Ptr clientAddress, CARD16 connectionType);
extern void ForEachChooserHost (ARRAY8Ptr clientAddress, CARD16 connectionType, ChooserFunc function, char *closure);

/* in choose.c */
extern ARRAY8Ptr IndirectChoice (ARRAY8Ptr clientAddress, CARD16 connectionType);
extern int IsIndirectClient (ARRAY8Ptr clientAddress, CARD16 connectionType);
extern int RememberIndirectClient (ARRAY8Ptr clientAddress, CARD16 connectionType);
extern void ForgetIndirectClient ( ARRAY8Ptr clientAddress, CARD16 connectionType);
extern void ProcessChooserSocket (int fd);
extern void RunChooser (struct display *d);

#endif /* XDMCP */

/* in daemon.c */
extern void BecomeDaemon (void);

/* in dm.c */
extern char prog[];
extern void CloseOnFork (void);
extern void RegisterCloseOnFork (int fd);
extern void StartDisplay (struct display *d);
extern void SetTitle (char *name, ...);
extern int ReStr (char **dst, const char *src);
extern int StrDup (char **dst, const char *src);
extern int StrApp (char **dst, const char *src);

/* in dpylist.c */
extern int AnyDisplaysLeft (void);
extern void ForEachDisplay (void (*f)(struct display *));
extern void RemoveDisplay (struct display *old);

/* in file.c */
extern void ParseDisplay (char *source);

#ifdef XDMCP

/* in netaddr.c */
extern char *NetaddrAddress(XdmcpNetaddr netaddrp, int *lenp);
extern char *NetaddrPort(XdmcpNetaddr netaddrp, int *lenp);
extern int ConvertAddr (XdmcpNetaddr saddr, int *len, char **addr);
extern int NetaddrFamily (XdmcpNetaddr netaddrp);
extern int addressEqual (XdmcpNetaddr a1, int len1, XdmcpNetaddr a2, int len2);

/* in policy.c */
#if 0
extern ARRAY8Ptr Accept (/* struct sockaddr *from, int fromlen, CARD16 displayNumber */);
#endif
extern ARRAY8Ptr ChooseAuthentication (ARRAYofARRAY8Ptr authenticationNames);
extern int CheckAuthentication (struct protoDisplay *pdpy, ARRAY8Ptr displayID, ARRAY8Ptr name, ARRAY8Ptr data);
extern int SelectAuthorizationTypeIndex (ARRAY8Ptr authenticationName, ARRAYofARRAY8Ptr authorizationNames);
extern int SelectConnectionTypeIndex (ARRAY16Ptr connectionTypes, ARRAYofARRAY8Ptr connectionAddresses);
extern int Willing (ARRAY8Ptr addr, CARD16 connectionType, ARRAY8Ptr authenticationName, ARRAY8Ptr status, xdmOpCode type);

/* in protodpy.c */
extern void DisposeProtoDisplay(struct protoDisplay *pdpy);

#endif /* XDMCP */

/* in reset.c */
extern void pseudoReset (Display *dpy);

/* in resource.c */
extern void InitResources (int argc, char **argv);
extern void LoadDMResources (void);
extern void LoadServerResources (struct display *d);
extern void LoadSessionResources (struct display *d);
extern void ReinitResources (void);

/* in session.c */
#ifdef USE_PAM
extern pam_handle_t **thepamh(void);
#endif
extern char **defaultEnv (void);
extern char **systemEnv (struct display *d, char *user, char *home);
extern int PingServer(struct display *d, Display *alternateDpy);
extern int source (char **environ, char *file);
extern void ClearCloseOnFork (int fd);
extern void DeleteXloginResources (struct display *d, Display *dpy);
extern void LoadXloginResources (struct display *d);
extern void ManageSession (struct display *d);
extern void SecureDisplay (struct display *d, Display *dpy);
extern void SessionExit (struct display *d, int status, int removeAuth);
extern void SessionPingFailed (struct display *d);
extern void SetupDisplay (struct display *d);
extern void StopDisplay (struct display *d);
extern void UnsecureDisplay (struct display *d, Display *dpy);
extern void WaitForChild (void);
extern void execute(char **argv, char **environ);

/* server.c */
extern char *_SysErrorMsg (int n);
extern int StartServer (struct display *d);
extern int WaitForServer (struct display *d);
extern void ResetServer (struct display *d);

/* socket.c */
extern int GetChooserAddr (char *addr, int *lenp);
extern void CreateWellKnownSockets (void);

/* in util.c */
extern char *localHostname (void);
extern char **parseArgs (char **argv, char *string);
extern char **setEnv (char **e, char *name, char *value);
extern char **putEnv(const char *string, char **env);
extern char *getEnv (char **e, char *name);
extern void CleanUpChild (void);
extern void freeArgs (char **argv);
extern void freeEnv (char **env);
extern void printEnv (char **e);

#ifdef XDMCP

/* in xdmcp.c */
extern char *NetworkAddressToHostname (CARD16 connectionType, ARRAY8Ptr connectionAddress);
extern int AnyWellKnownSockets (void);
extern void DestroyWellKnownSockets (void);
extern void SendFailed (struct display *d, char *reason);
extern void WaitForSomething (void);
extern void init_session_id(void);
extern void registerHostname(char *name, int namelen);

#endif /* XDMCP */

/*
 * CloseOnFork flags
 */

# define CLOSE_ALWAYS	    0
# define LEAVE_FOR_DISPLAY  1

#ifndef X_NOT_STDC_ENV
# include <stdlib.h>
#else
char *malloc(), *realloc();
#endif

#if defined(X_NOT_POSIX) && defined(SIGNALRETURNSINT)
# define SIGVAL int
#else
# define SIGVAL void
#endif

#if defined(X_NOT_POSIX) || defined(__EMX__) || defined(__NetBSD__) && defined(__sparc__)
# if defined(SYSV) || defined(__EMX__)
#  define SIGNALS_RESET_WHEN_CAUGHT
#  define UNRELIABLE_SIGNALS
# endif
# define Setjmp(e)	setjmp(e)
# define Longjmp(e,v)	longjmp(e,v)
# define Jmp_buf	jmp_buf
#else
# define Setjmp(e)	sigsetjmp(e,1)
# define Longjmp(e,v)	siglongjmp(e,v)
# define Jmp_buf	sigjmp_buf
#endif

typedef SIGVAL (*SIGFUNC)(int);

SIGVAL (*Signal(int, SIGFUNC Handler))(int);

#ifdef MINIX
# include <sys/nbio.h>
void udp_read_cb(nbio_ref_t ref, int res, int err);
void tcp_listen_cb(nbio_ref_t ref, int res, int err);
#endif

#ifdef __cplusplus
}
#endif

#endif /* _DM_H_ */
