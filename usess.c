#include "defs.h"
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>

//----------------------------------------------------------------------

static void QuitSignal (int sig);
static void AlarmSignal (int sig);
static void XreadySignal (int sig);
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
    // Set session signal handlers that quit
    signal (SIGHUP, QuitSignal);
    signal (SIGTERM, QuitSignal);
    signal (SIGALRM, AlarmSignal);
    signal (SIGUSR1, XreadySignal);

    // Give ownership of the tty
    fchown (STDIN_FILENO, acct->uid, acct->gid);
    fchmod (STDIN_FILENO, 0600);

    // Check if need to launch X
    char xinitrcPath [PATH_MAX];
    snprintf (xinitrcPath, sizeof(xinitrcPath), "%s/.xinitrc", acct->dir);
    bool useX = (0 == access (xinitrcPath, R_OK));

    pid_t xpid = (useX ? LaunchX (acct) : 0);
    pid_t shellpid = LaunchShell (acct, (useX && xpid) ? ".xinitrc" : NULL);

    while (shellpid || xpid) {
	int chldstat = 0;
	pid_t cpid = wait (&chldstat);
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

    // Retake ownership of the tty
    fchown (STDIN_FILENO, getuid(), getgid());

    alarm (0);
}

static void QuitSignal (int sig __attribute__((unused)))
{
    _quitting = true;
    alarm (KILL_TIMEOUT);
}

static void AlarmSignal (int sig __attribute__((unused)))
{
    _quitting = true;
    _killsig = SIGKILL;
}

static void XreadySignal (int sig __attribute__((unused)))
{
    _xready = true;
}

static void BecomeUser (const struct account* acct)
{
    if (0 != setgid (acct->gid))
	perror ("setgid");
    if (0 != setuid (acct->uid))
	perror ("setuid");

    clearenv();
    setenv ("TERM", "linux", false);
    setenv ("PATH", "/bin:/usr/bin", false);
    setenv ("USER", acct->name, true);
    setenv ("SHELL", acct->shell, true);
    setenv ("HOME", acct->dir, true);

    if (0 != chdir (acct->dir))
	perror ("chdir");
}

static pid_t LaunchX (const struct account* acct)
{
    pid_t pid = fork();
    if (pid > 0) {
	for (;;) {	// Wait for SIGUSR1 from X before returning
	    int ecode, rc = waitpid (pid, &ecode, 0);
	    if (rc == pid)	// X quit before signalling ready
		ExitWithMessage ("X failed to start");
	    else if (errno != EINTR)
		ExitWithError ("waitpid");
	    else if (_xready) {
		setenv ("DISPLAY", ":0", true);
		return (pid);
	    }
	}
	return (pid);
    } else if (pid < 0)
	ExitWithError ("fork");
    BecomeUser (acct);

    signal (SIGUSR1, SIG_IGN);	// This tells the X server to send SIGUSR1 to parent when ready

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

    char shname [16];
    const char* shbasename = strrchr(acct->shell, '/');
    if (!shbasename++)
	shbasename = acct->shell;
    snprintf (shname, sizeof(shname), "-%s", shbasename);
    const char* argv[] = { shname, arg, NULL };
    execvp (acct->shell, (char* const*) argv);
    ExitWithError ("execvp");
}
