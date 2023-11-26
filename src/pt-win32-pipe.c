/* Copyright (C) Gabor Karsay 2020 <gabor.karsay@gmx.at>
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include <windows.h>
#include <gtk/gtk.h>
#include <parlatype.h>
#include "pt-window.h"
#include "pt-win32-helpers.h"
#include "pt-win32-pipe.h"

struct _PtWin32PipePrivate
{
  HANDLE pipe;
  guint conn;
  GThread *thread;
};

#define PIPE_NAME "\\\\.\\pipe\\org.parlatype.ipc"
#define BUFSIZE 1024

G_DEFINE_TYPE_WITH_PRIVATE (PtWin32Pipe, pt_win32_pipe, PT_CONTROLLER_TYPE)

static char *
get_answer_for_request (PtWin32Pipe *self,
                        char *request)
{
  PtPlayer *player = pt_controller_get_player (PT_CONTROLLER (self));
  char *answer = NULL;

  if (g_strcmp0 (request, "GetURI") == 0)
    {
      answer = pt_player_get_uri (player);
    }
  else if (g_strcmp0 (request, "GetTimestamp") == 0)
    {
      answer = pt_player_get_timestamp (player);
    }
  else
    {
      answer = g_strdup ("Unknown request");
    }

  if (!answer)
    answer = g_strdup ("NONE");
  return answer;
}

static gpointer
start_server (gpointer data)
{
  PtWin32Pipe *self = PT_WIN32_PIPE (data);

  DWORD type = 0;
  BOOL success;
  DWORD numread = 0;

  char *error;
  char *request;
  char *answer;

  type = PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT;

  self->priv->pipe = CreateNamedPipe (PIPE_NAME,          /* unique name */
                                      PIPE_ACCESS_DUPLEX, /* read/write */
                                      type,
                                      PIPE_UNLIMITED_INSTANCES,
                                      BUFSIZE, /* output buffer size */
                                      BUFSIZE, /* input buffer size */
                                      0,       /* client time-out */
                                      NULL);   /* default security */

  if (self->priv->pipe == INVALID_HANDLE_VALUE)
    {
      error = pt_win32_get_last_error_msg ();
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
                        "MESSAGE", "Creating message pipe failed: %s",
                        error);
      g_free (error);
      return NULL;
    }

  g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                    "MESSAGE", "Pipe created");

  while (TRUE)
    {
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                        "MESSAGE", "Waiting for client");

      /* ConnectNamedPipe is blocking forever and a minute */
      self->priv->conn = ConnectNamedPipe (self->priv->pipe, NULL); /* NULL for sync operations */

      if (self->priv->conn == 0)
        {
          error = pt_win32_get_last_error_msg ();
          g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
                            "MESSAGE", "Client not connected: %s", error);
          g_free (error);
          return NULL;
        }

      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                        "MESSAGE", "Client connected");

      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                        "MESSAGE", "Listening for Client");

      request = malloc (BUFSIZE * sizeof (char));
      success = ReadFile (self->priv->pipe,        /* handle to pipe */
                          request,                 /* buffer to receive data */
                          BUFSIZE * sizeof (char), /* size of buffer */
                          &numread,                /* number of bytes read */
                          NULL);                   /* not overlapped I/O */

      if (!success || numread == 0)
        {
          if (GetLastError () == ERROR_BROKEN_PIPE)
            {
              g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                                "MESSAGE", "Client disconnected");
            }
          else
            {
              error = pt_win32_get_last_error_msg ();
              g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
                                "MESSAGE", "Error reading request: %s",
                                error);
              g_free (error);
            }
          return NULL;
        }

      /* Request is sometimes too long */
      if (strlen (request) > numread)
        request[numread] = '\0';
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                        "MESSAGE", "Client sent request: %s", request);

      if (g_strcmp0 (request, "QUIT") == 0)
        {
          FlushFileBuffers (self->priv->pipe);
          DisconnectNamedPipe (self->priv->pipe);
          CloseHandle (self->priv->pipe);
          return NULL;
        }

      answer = get_answer_for_request (self, request);
      g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                        "MESSAGE", "Sending answer: %s", answer);
      g_free (request);
      request = NULL;

      numread = 0;
      if (!WriteFile (self->priv->pipe, answer, strlen (answer) * sizeof (char), &numread, NULL))
        {
          error = pt_win32_get_last_error_msg ();
          g_log_structured (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
                            "MESSAGE", "Error sending answer: %d", error);
          g_free (error);
        }

      FlushFileBuffers (self->priv->pipe);
      DisconnectNamedPipe (self->priv->pipe);
      self->priv->conn = 0;
      g_free (answer);
      answer = NULL;
    }
}

void
pt_win32_pipe_start (PtWin32Pipe *self)
{
  /* All requests are handled in a single thread with a loop.
   * The thread returns only on errors or when the "QUIT" command is sent. */

  self->priv->thread = g_thread_new ("single-thread", start_server, self);
}

void
pt_win32_pipe_stop (PtWin32Pipe *self)
{
  DWORD mode, written;
  char *message;
  HANDLE closeconn;

  if (self->priv->pipe != INVALID_HANDLE_VALUE)
    {
      closeconn = CreateFile (PIPE_NAME,
                              GENERIC_READ | GENERIC_WRITE,
                              0, NULL, OPEN_EXISTING, 0, NULL);
      mode = PIPE_READMODE_MESSAGE;
      SetNamedPipeHandleState (closeconn, &mode, NULL, NULL);
      message = "QUIT";
      WriteFile (closeconn,
                 message,
                 sizeof (char) * (strlen (message) + 1),
                 &written, NULL);
      CloseHandle (closeconn);
    }
}

static void
pt_win32_pipe_init (PtWin32Pipe *self)
{
  self->priv = pt_win32_pipe_get_instance_private (self);

  self->priv->pipe = INVALID_HANDLE_VALUE;
  self->priv->conn = 0;
}

static void
pt_win32_pipe_dispose (GObject *object)
{
  PtWin32Pipe *self = PT_WIN32_PIPE (object);

  pt_win32_pipe_stop (self);
  g_thread_join (self->priv->thread);

  if (self->priv->conn > 0)
    {
      FlushFileBuffers (self->priv->pipe);
      DisconnectNamedPipe (self->priv->pipe);
      self->priv->conn = 0;
    }

  if (self->priv->pipe != INVALID_HANDLE_VALUE)
    CloseHandle (self->priv->pipe);

  G_OBJECT_CLASS (pt_win32_pipe_parent_class)->dispose (object);
}

static void
pt_win32_pipe_class_init (PtWin32PipeClass *klass)
{
  G_OBJECT_CLASS (klass)->dispose = pt_win32_pipe_dispose;
}

PtWin32Pipe *
pt_win32_pipe_new (PtWindow *win)
{
  return g_object_new (PT_WIN32_PIPE_TYPE, "win", win, NULL);
}
