#include "defs.h"
#include <sys/wait.h>

//----------------------------------------------------------------------

static void QuitSignal (int sig);
static void AlarmSignal (int sig);
static void XreadySignal (int sig);
static void ChildSignal (int sig);
static void BecomeUser (const struct account* acct);
static pid_t LaunchX (const struct account* acct);
static pid_t LaunchShell (const struct account* acct, const char* arg);

//----------------------------------------------------------------------

static bool _quitting = false;
static bool _xready = false;
static int _killsig = SIGTERM;

//----------------------------------------------------------------------

void RunSession (const struct account* acct)
{
    // Check if need to launch X
    char xinitrcPath [PATH_MAX];
    snprintf (xinitrcPath, sizeof(xinitrcPath), "%s/.xinitrc", acct->dir);

    pid_t xpid = 0;
    if (0 == access (xinitrcPath, R_OK))
	xpid = LaunchX (acct);
    pid_t shellpid = LaunchShell (acct, xpid ? ".xinitrc" : NULL);

    // Set session signal handlers that quit
    typedef void (*psigfunc_t)(int);
    psigfunc_t hupsig = signal (SIGHUP, QuitSignal);
    psigfunc_t termsig = signal (SIGTERM, QuitSignal);
    psigfunc_t quitsig = signal (SIGQUIT, QuitSignal);
    psigfunc_t alrmsig = signal (SIGALRM, AlarmSignal);
    signal (SIGCHLD, ChildSignal);

    sigset_t smask;
    sigprocmask (SIG_UNBLOCK, NULL, &smask);

    while (shellpid || xpid) {
	sigsuspend (&smask);
	int chldstat = 0;
	pid_t cpid = waitpid (-1, &chldstat, WNOHANG);
	if (cpid == shellpid || cpid == xpid) {
	    if (cpid == shellpid)
		shellpid = 0;
	    else if (cpid == xpid)
		xpid = 0;
	    _quitting = true;
	    alarm (KILL_TIMEOUT);
	}
	if (_quitting) {
	    if (shellpid)
		kill (shellpid, _killsig);
	    if (xpid)
		kill (xpid, _killsig);
	}
    }

    // Restore main signal handlers
    signal (SIGCHLD, SIG_IGN);
    signal (SIGALRM, alrmsig);
    signal (SIGQUIT, termsig);
    signal (SIGTERM, quitsig);
    signal (SIGHUP, hupsig);
    alarm (0);
}

static void QuitSignal (int sig)
{
    syslog (LOG_INFO, "shutting down session on signal %d", sig);
    _quitting = true;
    alarm (KILL_TIMEOUT);
}

static void AlarmSignal (int sig __attribute__((unused)))
{
    syslog (LOG_WARNING, "session hung; switching to SIGKILL");
    _quitting = true;
    _killsig = SIGKILL;
}

static void XreadySignal (int sig __attribute__((unused)))
{
    _xready = true;
}

static void ChildSignal (int sig __attribute__((unused)))
{
}

static void BecomeUser (const struct account* acct)
{
    if (0 != setgid (acct->gid))
	perror ("setgid");
    if (0 != setuid (acct->uid))
	perror ("setuid");

    clearenv();
    setenv ("TERM", _termname, false);
    setenv ("PATH", _PATH_DEFPATH, false);
    setenv ("USER", acct->name, true);
    setenv ("SHELL", acct->shell, true);
    setenv ("HOME", acct->dir, true);

    if (0 != chdir (acct->dir))
	perror ("chdir");
}

static pid_t LaunchX (const struct account* acct)
{
    signal (SIGUSR1, XreadySignal);

    // Block delivery of SIGUSR1 until ready to avoid race conditions
    sigset_t busr, orig;
    sigemptyset (&busr);
    sigaddset (&busr, SIGUSR1);
    sigprocmask (SIG_BLOCK, &busr, &orig);

    pid_t pid = fork();
    if (pid > 0) {
	sigprocmask (SIG_SETMASK, &orig, NULL);	// Now unblock SIGUSR1
	for (;;) {
	    sigsuspend (&orig);	// Wait for SIGUSR1 from X before returning
	    int ecode, rc = waitpid (pid, &ecode, WNOHANG);
	    if (rc || errno != EINTR)
		return (0);	// X failed to start, fallback to plain shell
	    else if (_xready)
		break;
	}
	return (pid);
    } else if (pid < 0)
	ExitWithError ("fork");
    BecomeUser (acct);

    signal (SIGTTIN, SIG_IGN);	// Ignore server reads and writes
    signal (SIGTTOU, SIG_IGN);
    signal (SIGUSR1, SIG_IGN);	// This tells the X server to send SIGUSR1 to parent when ready
    sigprocmask (SIG_SETMASK, &orig, NULL);	// Now unblock SIGUSR1

    const char* argv[] = { "X", ":0", "-nolisten", "tcp", "-auth", ".config/Xauthority", NULL };
    if (0 != access (argv[5], R_OK))
	argv[4] = NULL;
    execvp ("/usr/bin/X", (char* const*) argv);
    ExitWithError ("execvp");
}

static pid_t LaunchShell (const struct account* acct, const char* arg)
{
    pid_t pid = fork();
    if (pid > 0)
	return (pid);
    else if (pid < 0)
	ExitWithError ("fork");
    BecomeUser (acct);

    if (arg) {	// If launching xinitrc, set DISPLAY
	setenv ("DISPLAY", ":0", true);
	char xauthpath [PATH_MAX];
	snprintf (xauthpath, sizeof(xauthpath), "%s/.config/Xauthority", acct->dir);
	setenv ("XAUTHORITY", xauthpath, true);
    }

    char shname [16];	// argv[0] of a login shell is "-bash"
    const char* shbasename = strrchr(acct->shell, '/');
    if (!shbasename++)
	shbasename = acct->shell;
    snprintf (shname, sizeof(shname), "-%s", shbasename);

    const char* argv[] = { shname, arg, NULL };
    execvp (acct->shell, (char* const*) argv);
    ExitWithError ("execvp");
}
