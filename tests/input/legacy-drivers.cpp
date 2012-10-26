#if HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdexcept>
#include <fstream>
#include <xorg/gtest/xorg-gtest.h>

#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <X11/Xlib.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>

#include "input-driver-test.h"
#include "helpers.h"

/* no implementation, this class only exists for better test names */
class LegacyInputDriverTest : public SimpleInputDriverTest {};

static int count_devices(Display *dpy) {
    int ndevices;
    XIDeviceInfo *info;

    info = XIQueryDevice(dpy, XIAllDevices, &ndevices);
    XIFreeDeviceInfo(info);
    return ndevices;
}

TEST_P(LegacyInputDriverTest, InputDeviceSectionSimple)
{
    std::string param;

    param = GetParam();

    int expected_devices;

    /* apparently when the acecad driver is loaded neither mouse nor kbd
     * loads - probably a bug but that's current behaviour on RHEL 6.3 */
    if (param.compare("acecad") == 0)
        expected_devices = 4;
    else {
        expected_devices = 6;
	/* xserver git 1357cd7251 , no <default pointer> */
	if (server.GetVersion().compare("1.11.0") > 0)
		expected_devices--;
    }

    ASSERT_EQ(count_devices(Display()), expected_devices);

    /* No joke, they all fail. some fail for missing Device option, others
     * for not finding the device, etc. */
    ASSERT_EQ(FindInputDeviceByName(Display(), "--device--"), 0);
}

INSTANTIATE_TEST_CASE_P(, LegacyInputDriverTest,
        ::testing::Values("acecad", "aiptek", "elographics",
                          "fpit", "hyperpen",  "mutouch",
                          "penmount"));

/**
 * Void input driver test class
 */
class VoidTest : public InputDriverTest {
public:
    /**
     * Initialize an xorg.conf with a single CorePointer void device.
     */
    virtual void SetUpConfigAndLog() {

        config.AddDefaultScreenWithDriver();
        config.AddInputSection("void", "--device--", "Option \"CorePointer\" \"on\"\n");
        config.WriteConfig();
        server.SetOption("-config", config.GetPath());
    }
};

TEST_F(VoidTest, InputDeviceSectionSimple)
{
    ASSERT_EQ(FindInputDeviceByName(Display(), "--device--"), 1);
}


/***********************************************************************
 *                                                                     *
 *                               ACECAD                                *
 *                                                                     *
 ***********************************************************************/
TEST(AcecadTest, InputDeviceSectionWithOptionDevice)
{
    XOrgConfig config;
    XITServer server;

    config.AddInputSection("acecad", "--device--",
                           "Option \"CorePointer\" \"on\"\n"
                           "Option \"Device\" \"/dev/input/event0\"\n");
    config.AddDefaultScreenWithDriver();
    StartServer("acecad-type-stylus", server, config);

    ::Display *dpy = XOpenDisplay(server.GetDisplayString().c_str());
    int major = 2;
    int minor = 0;
    ASSERT_EQ(Success, XIQueryVersion(dpy, &major, &minor));

    int ndevices;
    XIDeviceInfo *info;

    info = XIQueryDevice(dpy, XIAllDevices, &ndevices);
    XIFreeDeviceInfo(info);

    /* VCP, VCK, xtest, mouse, keyboard, acecad */
    int expected_devices = 7;

    /* xserver git 1357cd7251 , no <default pointer> */
    if (server.GetVersion().compare("1.11.0") > 0)
	    expected_devices--;

    ASSERT_EQ(count_devices(dpy), expected_devices);
    ASSERT_EQ(FindInputDeviceByName(dpy, "--device--"), 1);

    config.RemoveConfig();
    server.RemoveLogFile();
}

/***********************************************************************
 *                                                                     *
 *                               AIPTEK                                *
 *                                                                     *
 ***********************************************************************/

class AiptekTest : public InputDriverTest,
                   public ::testing::WithParamInterface<std::string> {
    virtual void SetUpConfigAndLog() {

        std::string type = GetParam();

        config.AddInputSection("aiptek", "--device--",
                "Option \"CorePointer\" \"on\"\n"
                "Option \"Device\" \"/dev/input/event0\"\n"
                "Option \"Type\" \"" + type + "\"");
        config.AddDefaultScreenWithDriver();
        config.WriteConfig();
    }
};

TEST_P(AiptekTest, InputDeviceSectionWithType)
{
    ::Display *dpy = Display();

    /* VCP, VCK, xtest, mouse, keyboard, aiptek */
    int expected_devices = 7;

    /* xserver git 1357cd7251, no <default pointer> */
    if (server.GetVersion().compare("1.11.0") > 0)
	    expected_devices--;

    ASSERT_EQ(count_devices(dpy), expected_devices);
    ASSERT_EQ(FindInputDeviceByName(dpy, "--device--"), 1);

    config.RemoveConfig();
    server.RemoveLogFile();
}

INSTANTIATE_TEST_CASE_P(, AiptekTest,
                        ::testing::Values("stylus", "cursor", "eraser"));

/***********************************************************************
 *                                                                     *
 *                            ELOGRAPHICS                              *
 *                                                                     *
 ***********************************************************************/
TEST(ElographicsTest, InputDeviceSectionWithOptionDevice)
{
    XOrgConfig config;
    XITServer server;

    config.AddInputSection("elographics", "--device--",
                           "Option \"CorePointer\" \"on\"\n"
                           "Option \"Device\" \"/dev/input/event0\"\n");
    config.AddDefaultScreenWithDriver();
    StartServer("elographics", server, config);

    ::Display *dpy = XOpenDisplay(server.GetDisplayString().c_str());
    int major = 2;
    int minor = 0;
    ASSERT_EQ(Success, XIQueryVersion(dpy, &major, &minor));

    /* VCP, VCK, xtest, mouse, keyboard, aiptek */
    int expected_devices = 7;

    /* xserver git 1357cd7251, no <default pointer> */
    if (server.GetVersion().compare("1.11.0") > 0)
	    expected_devices--;

    ASSERT_EQ(count_devices(dpy), expected_devices);

    int deviceid;
    if (FindInputDeviceByName(dpy, "--device--", &deviceid) != 1) {
        SCOPED_TRACE("\n"
                     "	Elographics device '--device--' not found.\n"
                     "	Maybe this is elographics < 1.4.\n"
                     "	Checking for TOUCHSCREEN instead, see git commit\n"
                     "	xf86-input-elographics-1.3.0-1-g55f337f");
        ASSERT_EQ(FindInputDeviceByName(dpy, "TOUCHSCREEN", &deviceid), 1);
    } else
        ASSERT_EQ(FindInputDeviceByName(dpy, "--device--", &deviceid), 1);

    int ndevices;
    XIDeviceInfo *info = XIQueryDevice(dpy, deviceid, &ndevices);
    ASSERT_EQ(ndevices, 1);

    ASSERT_EQ(info->use, XISlavePointer) << "https://bugs.freedesktop.org/show_bug.cgi?id=40870";
    ASSERT_EQ(info->attachment, 2); /* VCP */

    XIFreeDeviceInfo(info);

    config.RemoveConfig();
    server.RemoveLogFile();
}

/* Create a named pipe that pumps out elographics 10-byte packets */
struct elo_packet {
    unsigned char header;               /* always 'U' */
    unsigned char expected;             /* always 'T' (touch packet) in this program */
    unsigned char has_pressure;         /* 0x80 if pressure is present, 0 otherwise */
    unsigned char x_lo;
    unsigned char x_hi;
    unsigned char y_lo;
    unsigned char y_hi;
    unsigned char pressure_hi;
    unsigned char pressure_lo;
    unsigned char checksum;             /* 0x88 + bytes[1..8] */
};

static void elo_init_packet(struct elo_packet *packet, short x, short y, short pressure)
{
    int i;

    packet->header = 'U';
    packet->expected = 'T';
    packet->has_pressure = 0x80;
    packet->x_lo = x & 0xFF;
    packet->x_hi = (x & 0xFF00) >> 8;
    packet->y_lo = y & 0xFF;
    packet->y_hi = (y & 0xFF00) >> 8;
    packet->pressure_lo = pressure & 0xFF;
    packet->pressure_hi = (pressure & 0xFF00) >> 8;

    packet->checksum = 0xaa;

    for (i = 0; i < 9; i++)
        packet->checksum += *(((unsigned char*)packet) + i);
}

static void elo_sighandler(int sig)
{
    unlink("/tmp/elographics.pipe");
    exit(0);
}

static void elo_sighandler_pipe(int sig)
{
}

int elo_simulate(void)
{
    int i;
    int rc, fd;
    struct elo_packet packet;
    const char *path = "/tmp/elographics.pipe";

    rc = mkfifo(path, 0x755);
    if (rc == -1) {
        perror("Failed to create pipe:");
        return 1;
    }

    signal(SIGINT, elo_sighandler);
    signal(SIGTERM, elo_sighandler);
    signal(SIGPIPE, elo_sighandler_pipe);

    fd = open(path, O_WRONLY);
    if (fd == -1) {
        perror("Failed to open pipe:");
        rc = 1;
        goto out;
    }

    for (i = 0; i < 30; i++) {
        elo_init_packet(&packet, 100 * i, 10, 10);
        rc = write(fd, &packet, sizeof(packet));
        if (rc == -1 && errno == SIGPIPE)
            i--;
        usleep(500000);
    }

out:
    unlink(path);

    return rc;
}

TEST(ElographicsTest, StylusMovement)
{
    XOrgConfig config;
    XITServer server;

    config.AddInputSection("elographics", "--device--",
                           "Option \"CorePointer\" \"on\"\n"
                           "Option \"DebugLevel\" \"10\"\n"
                           "Option \"Model\" \"Sunit dSeries\"" /* does not send packets to device*/
                           "Option \"Device\" \"/tmp/elographics.pipe\"\n");
    config.AddDefaultScreenWithDriver();

    /* fork here */
    pid_t pid = fork();
    if (pid == 0) { /* child */
#ifdef __linux
        prctl(PR_SET_PDEATHSIG, SIGTERM);
#endif
        elo_simulate();
        return;
    }

    /* parent */
    StartServer("elographics", server, config);

    ::Display *dpy = XOpenDisplay(server.GetDisplayString().c_str());
    int major = 2;
    int minor = 0;
    ASSERT_EQ(Success, XIQueryVersion(dpy, &major, &minor));

    XSelectInput(dpy, DefaultRootWindow(dpy), PointerMotionMask);
    XSync(dpy, False);

    if (FindInputDeviceByName(dpy, "--device--") != 1) {
        SCOPED_TRACE("\n"
                     "	Elographics device '--device--' not found.\n"
                     "	Maybe this is elographics < 1.4.\n"
                     "	Checking for TOUCHSCREEN instead, see git commit\n"
                     "	xf86-input-elographics-1.3.0-1-g55f337f");
        ASSERT_EQ(FindInputDeviceByName(dpy, "TOUCHSCREEN"), 1);
    } else
        ASSERT_EQ(FindInputDeviceByName(dpy, "--device--"), 1);

    ASSERT_EQ(xorg::testing::XServer::WaitForEventOfType(dpy, MotionNotify, -1, -1, 1000), true);

    XEvent first, second;
    XNextEvent(dpy, &first);

    ASSERT_EQ(xorg::testing::XServer::WaitForEventOfType(dpy, MotionNotify, -1, -1, 1000), true);
    XNextEvent(dpy, &second);

    ASSERT_LT(first.xmotion.x_root, second.xmotion.x_root);

    config.RemoveConfig();
    server.RemoveLogFile();
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
