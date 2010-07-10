/*
 * Copyright (c) 2010 Øyvind Kolås <pippin@gimp.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <clutter/clutter.h>
#include <lo/lo.h>

#if 0

gfloat starty = 0;

static gboolean
drag_capture (ClutterActor *stage,
              ClutterEvent *event,
              gpointer      data)
{
  Control *control = data;
  ClutterActor *actor = control->actor;
  switch (event->any.type)
    {
      case CLUTTER_MOTION:
        {
          gfloat dx, dy;
          gfloat x, y;

          dx=prevx-event->motion.x;
          dy=prevy-event->motion.y;

          clutter_actor_get_position (actor, &x, &y);
          y = starty - dy;
          if (y > 10 && y < height + 10)
            {
              gfloat val;
              val = 1.0 - ((y - 10)/height);
              val = val * (control->max-control->min) + control->min;
  
              if (control->no == 0)
                {
                  clutter_actor_set_position (actor, x, y);
                  lo_send (synth_address, "/ui-query", "i", (int)val);
                  curr_preset = (int)val;
                }
              lo_send (synth_address, "/set", "iif", curr_preset, control->no - 1, val);
            }
        }
        return TRUE;
        break;
      case CLUTTER_BUTTON_PRESS:
        return TRUE;
      case CLUTTER_BUTTON_RELEASE:
        g_signal_handlers_disconnect_by_func (stage, drag_capture, data);
        return TRUE;
        break;
      default:
        break;
    }
  return FALSE;
}

static gboolean drag_start (ClutterActor *actor,
                            ClutterEvent *event,
                            gpointer      userdata)
{
  prevx = event->button.x;
  prevy = event->button.y;
  starty = clutter_actor_get_y (actor);

  g_signal_connect (clutter_actor_get_stage (actor), "captured-event",
                    G_CALLBACK (drag_capture), userdata);
  return TRUE;
}

static GList *pressed = NULL; /* list of pressed keys, to disable keyrepeat */
static gboolean key_press (ClutterActor    *actor,
                           ClutterKeyEvent *e,
                           gpointer         data)
{
  if (g_list_find (pressed, GINT_TO_POINTER (e->keyval)))
    return TRUE;
  pressed = g_list_prepend (pressed, GINT_TO_POINTER (e->keyval));

  if (e->keyval < 200)
  lo_send (synth_address, "/note-on", "iff", curr_preset, e->keyval-30.0, 1.0);
  return TRUE;
}
static gboolean key_release (ClutterActor    *actor,
                             ClutterKeyEvent *e,
                             gpointer         data)
{
  pressed = g_list_remove (pressed, GINT_TO_POINTER (e->keyval));

  lo_send (synth_address, "/note-off", "iff", curr_preset, e->keyval - 30.0, 1.0);
  return TRUE;
}
#endif

static lo_address synth_address = NULL;

int
osc_log (const char  *path,
         const char  *types,
         lo_arg     **argv,
         int         argc,
         void        *data,
         void        *synth)
{
    int i;

    g_print ("OSC %s ", path);
    for (i=0; i<argc; i++)
      {
	g_print (" ");
	lo_arg_pp(types[i], argv[i]);
      }
    g_print("\n");
    return 1; /* message not handled */
}

static void
osc_error (int         num,
           const char *msg,
           const char *path)
{
  g_print ("liblo server error %d in path %s: %s\n", num, path, msg);
}

static gboolean text_changed (ClutterText *self,
                              void        *data)
{
  lo_send (synth_address, "/kill", "i", GPOINTER_TO_INT (data));
  lo_send (synth_address, "/run", "is", GPOINTER_TO_INT (data), clutter_text_get_text (self));
  lo_send (synth_address, "/release", "i", GPOINTER_TO_INT (data));
  return TRUE;
}

ClutterActor *voice_editor_new (int no)
{
  ClutterActor *group, *name, *code;

  name = g_object_new (CLUTTER_TYPE_TEXT,
                      "editable", TRUE,
                      "reactive", TRUE,
                      "text", "unnamed",
                      "x", 4.0,
                      "y", 4.0,
                      NULL);
  code = g_object_new (CLUTTER_TYPE_TEXT,
                      "editable", TRUE,
                      "reactive", TRUE,
                      "text", "sin(hz=440) * adsr(0.1, 0.5, 0.4, 0.8)",
                      "x", 4.0,
                      "y", 24.0,
                      NULL);
  g_signal_connect (code, "text-changed",
                    G_CALLBACK (text_changed), GINT_TO_POINTER (no));
  group = clutter_group_new ();
  clutter_container_add (CLUTTER_CONTAINER (group), name, code, NULL);
  return group;
}

int main (int argc, gchar **argv)
{
  ClutterActor *stage;
  ClutterActor *voice_editor[4];
  int i;

  lo_server_thread st = lo_server_thread_new ("1978", osc_error);
  lo_server_thread_add_method (st, NULL, NULL, osc_log, NULL);
  lo_server_thread_start (st);

  synth_address = lo_address_new ("localhost", "6150");

  clutter_init (&argc, &argv);
  stage = CLUTTER_ACTOR (clutter_stage_new ());

  for (i = 0; i < 4; i++)
    {
      voice_editor[i] = voice_editor_new (i);
      clutter_container_add_actor (CLUTTER_CONTAINER (stage), voice_editor[i]);
      clutter_actor_set_y (voice_editor[i],
                           i*100);
    }

  clutter_actor_show (stage);
  clutter_main ();
  return 0;
}
