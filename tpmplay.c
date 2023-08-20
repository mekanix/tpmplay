#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <swtpm/tpm_ioctl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#define devtoh32(x) be32toh(x)
#define htodev32(x) htobe32(x)

#define devtoh64(x) be64toh(x)
#define htodev64(x) htobe64(x)

#ifndef _IOC_NRSHIFT
#define _IOC_NRSHIFT 0
#endif
#ifndef _IOC_NRMASK
#define _IOC_NRMASK 255
#endif

#define DEFAULT_POLL_TIMEOUT 10000 /* ms */

typedef struct channels {
  int ctrl;
  int data;
} channels_t;

static unsigned long ioctl_to_cmd(unsigned long ioctlnum) {
  /* the ioctl number contains the command number - 1 */
  return ((ioctlnum >> _IOC_NRSHIFT) & _IOC_NRMASK) + 1;
}

/*
 * ctrlcmd - send a control command
 *
 * This function returns -1 on on error with errno indicating the error.
 * It returns the number of bytes received in the response.
 */
static int ctrlcmd(int fd, unsigned long cmd, void *msg, size_t msg_len_in,
                   size_t msg_len_out)
{
  int rc;
  uint32_t cmd_no = htobe32(ioctl_to_cmd(cmd));
  struct iovec iov[2] = {
    {
      .iov_base = &cmd_no,
      .iov_len = sizeof(cmd_no),
    }, {
      .iov_base = msg,
      .iov_len = msg_len_in,
    },
  };

  rc = writev(fd, iov, 2);
  if (rc > 0) {
    if (msg_len_out > 0) {
      struct pollfd fds = {
        .fd = fd,
        .events = POLLIN,
      };
      rc = poll(&fds, 1, DEFAULT_POLL_TIMEOUT);
      if (rc == 1) {
        rc = read(fd, msg, msg_len_out);
      } else if (rc == 0) {
        rc = -1;
        errno = ETIMEDOUT;
      }
    } else {
      /* we read 0 bytes */
      rc = 0;
    }
  }
  return rc;
}


static channels_t * init(const char *directory) {
  int rc;
  size_t size;
  size_t chsize;
  char *channel;
  struct sockaddr_un addr;
  ptm_init cmd;
  ptm_res res;
  uint32_t cmd_no;
  channels_t *chs;

  chs = malloc(sizeof(channels_t));

  // ctrl
  channel = "/ctrl";
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  size = strlen(directory);
  chsize = strlen(channel);
  strncpy(addr.sun_path, directory, size);
  strncpy(addr.sun_path + size, channel, chsize);
  addr.sun_path[size + chsize] = '\0';
  if ((chs->ctrl = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    perror("ctrl socket");
    exit(1);
  }
  if (connect(chs->ctrl, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    perror("ctrl connect");
    exit(1);
  }
  cmd.u.req.init_flags = htodev32(PTM_INIT_FLAG_DELETE_VOLATILE);
  rc = ctrlcmd(chs->ctrl, PTM_INIT, &cmd, sizeof(cmd.u.req), sizeof(cmd.u.resp));
  if (rc < 0) {
    fprintf(stderr, "Could not execute PTM_INIT: %s\n", strerror(errno));
    exit(1);
  }
  res = devtoh32(cmd.u.resp.tpm_result);
  if (res != 0) {
    fprintf(stderr, "TPM result from PTM_INIT: 0x%x\n", res);
    exit(1);
  }

  // data
  channel = "/data";
  chsize = strlen(channel);
  strncpy(addr.sun_path + size, channel, chsize);
  addr.sun_path[size + chsize] = '\0';
  if ((chs->data = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    perror("data socket");
    exit(1);
  }
  if (connect(chs->data, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    perror("data connect");
    exit(1);
  }
  return chs;
}

static void deinit(channels_t *chs) {
  int rc;
  ptm_res res;

  rc = ctrlcmd(chs->ctrl, PTM_STOP, &res, 0, sizeof(res));
  if (rc < 0) {
    fprintf(stderr, "Could not execute PTM_STOP: %s\n", strerror(errno));
  }
  if (devtoh32(res) != 0) {
    fprintf(stderr, "TPM result from PTM_STOP: 0x%x\n", devtoh32(res));
  }
  close(chs->ctrl);
  close(chs->data);
  free(chs);
}

int main(int argc, char **argv) {
  channels_t *chs;

  if (argc != 2) {
    fprintf(stderr, "Usage: %s <path>\n", argv[0]);
    exit(1);
  }

  chs = init(argv[1]);

  // TODO: ctrl/data commands

  deinit(chs);
  chs = NULL;
  return 0;
}
