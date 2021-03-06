/* Ekiga -- A VoIP and Video-Conferencing application
 * Copyright (C) 2012, Xunta de Galicia <ocfloss@xunta.es>
 *
 * Author: Victor Jaquez, Igalia S.L., AGASOL. <vjaquez@igalia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *
 * Ekiga is licensed under the GPL license and as a special exception,
 * you have permission to link or otherwise combine this program with the
 * programs OPAL, OpenH323 and PWLIB, and distribute the combination,
 * without applying the requirements of the GNU GPL to the OPAL, OpenH323
 * and PWLIB programs, as long as you do follow the requirements of the
 * GNU GPL for all the rest of the software thus combined.
 */

#include "ext-window.h"
#include "ekiga-settings.h"

#ifndef WIN32
#include <gdk/gdkx.h>
#else
#include "platform/winpaths.h"
#include <gdk/gdkwin32.h>
#endif

#define STAGE_WIDTH 640
#define STAGE_HEIGHT 480

G_DEFINE_TYPE (EkigaExtWindow, ekiga_ext_window, GTK_TYPE_WINDOW);

struct _EkigaExtWindowPrivate {
#ifndef WIN32
  GC gc;
#endif
  GtkWidget *video, *zin, *zout, *event_box;
  ClutterActor *stage;
  ClutterActor *video_stream;
  boost::shared_ptr<Ekiga::VideoOutputCore> vocore;
  boost::shared_ptr<Ekiga::Settings> video_display_settings;
};

static void
set_zoom_buttons_sensitive (EkigaExtWindow *ew, guint zoom)
{
  gtk_widget_set_sensitive (ew->priv->zin, zoom != 200);
  gtk_widget_set_sensitive (ew->priv->zout, zoom != 50);
}

static void
zoom_in (G_GNUC_UNUSED GtkWidget *widget, gpointer user_data)
{
  guint zoom;
  EkigaExtWindow *ew = EKIGA_EXT_WINDOW (user_data);

  zoom = ew->priv->video_display_settings->get_int ( "ext_zoom");
  if (zoom < 200)
    zoom = zoom * 2;
  ew->priv->video_display_settings->set_int ("ext_zoom", zoom);

  set_zoom_buttons_sensitive (ew, zoom);
}

static void
zoom_out (G_GNUC_UNUSED GtkWidget *widget, gpointer user_data)
{
  guint zoom;
  EkigaExtWindow *ew = EKIGA_EXT_WINDOW (user_data);

  zoom = ew->priv->video_display_settings->get_int ( "ext_zoom");
  if (zoom > 50)
    zoom = (guint) zoom / 2;
  ew->priv->video_display_settings->set_int ("ext_zoom", zoom);

  set_zoom_buttons_sensitive (ew, zoom);
}

static void
stay_on_top_changed_cb (GSettings *settings,
                        gchar *key,
                        gpointer self)

{
  bool val = false;

  g_return_if_fail (self != NULL);

  val = g_settings_get_boolean (settings, key);
  gdk_window_set_keep_above (GDK_WINDOW (gtk_widget_get_window (GTK_WIDGET (self))), val);
}

static void
ekiga_extended_video_window_init_clutter (EkigaExtWindow *ew)
{
  GtkWidget *clutter_widget = NULL;

  clutter_widget = gtk_clutter_embed_new ();
  gtk_widget_set_size_request (GTK_WIDGET (clutter_widget), STAGE_WIDTH, STAGE_HEIGHT);
  gtk_container_add (GTK_CONTAINER (ew), clutter_widget);

  ew->priv->stage = gtk_clutter_embed_get_stage (GTK_CLUTTER_EMBED (clutter_widget));
  clutter_actor_set_background_color (CLUTTER_ACTOR (ew->priv->stage), CLUTTER_COLOR_Black);
  clutter_stage_set_user_resizable (CLUTTER_STAGE (ew->priv->stage), TRUE);

  ew->priv->video_stream =
    CLUTTER_ACTOR (g_object_new (CLUTTER_TYPE_TEXTURE, "disable-slicing", TRUE, NULL));
  clutter_actor_add_child (CLUTTER_ACTOR (ew->priv->stage), CLUTTER_ACTOR (ew->priv->video_stream));
  clutter_actor_add_constraint (ew->priv->video_stream,
                                clutter_align_constraint_new (ew->priv->stage,
                                                              CLUTTER_ALIGN_BOTH,
                                                              0.5));
}

static inline void
clear_display_info (EkigaExtWindow *ew)
{
  /*
  Ekiga::DisplayInfo info;

  info.x = 0;
  info.y = 0;
  info.widget_info_set = false;
  info.zoom = 0;
  info.mode = Ekiga::VO_MODE_UNSET;
  info.config_info_set = false;
#ifdef WIN32
  info.hwnd = 0;
#else
  info.gc = 0;
  info.window = 0;
#endif
*/
  // FIXME
  //ew->priv->vocore->set_ext_display_info (info);
}

static GObject *
constructor (GType type, guint n_properties, GObjectConstructParam *params)
{
  GObject *object;

  object = G_OBJECT_CLASS (ekiga_ext_window_parent_class)->constructor (type,
                                                                        n_properties,
                                                                        params);

  ekiga_extended_video_window_init_clutter (EKIGA_EXT_WINDOW (object));

  return object;
}

static void
finalize (GObject* gobject)
{
  EkigaExtWindow *ew = EKIGA_EXT_WINDOW (gobject);

  delete ew->priv;
  ew->priv = NULL;

  G_OBJECT_CLASS (ekiga_ext_window_parent_class)->finalize (gobject);
}

static gboolean
focus_in_event (GtkWidget *widget, GdkEventFocus *event)
{
  if (gtk_window_get_urgency_hint (GTK_WINDOW (widget)))
    gtk_window_set_urgency_hint (GTK_WINDOW (widget), false);

  return GTK_WIDGET_CLASS (ekiga_ext_window_parent_class)->focus_in_event (widget,
                                                                           event);
}

static void
show (GtkWidget *widget)
{
  EkigaExtWindow *ew = EKIGA_EXT_WINDOW (widget);
  GdkWindow *w = gtk_widget_get_window (widget);

  if (w && ew->priv->video_display_settings->get_bool ("stay-on-top"))
    gdk_window_set_keep_above (w, true);

  GTK_WIDGET_CLASS (ekiga_ext_window_parent_class)->show (widget);

  gtk_widget_queue_draw (widget);
}

static gboolean
draw_event (GtkWidget *widget,
            cairo_t *context)
{
  /*
  EkigaExtWindow *ew = EKIGA_EXT_WINDOW (widget);
  Ekiga::DisplayInfo info;
  gboolean handled;
  GtkAllocation alloc;

  handled = (*GTK_WIDGET_CLASS (ekiga_ext_window_parent_class)->draw) (widget, context);

  gtk_widget_get_allocation (ew->priv->video, &alloc);
  info.x = alloc.x;
  info.y = alloc.y;

#ifdef WIN32
  info.hwnd = (HWND) GDK_WINDOW_HWND (gtk_widget_get_window (ew->priv->video));
#else
  info.window = gdk_x11_window_get_xid (gtk_widget_get_window (ew->priv->video));
  g_return_val_if_fail (info.window != 0, handled);

  if (!ew->priv->gc) {
    Display *display;
    display = GDK_DISPLAY_XDISPLAY (gtk_widget_get_display (ew->priv->video));
    ew->priv->gc = XCreateGC(display, info.window, 0, 0);
    g_return_val_if_fail (ew->priv->gc != NULL, handled);
  }

  info.gc = ew->priv->gc;

  gdk_flush ();
#endif

  info.widget_info_set = TRUE;
  info.mode = Ekiga::VO_MODE_REMOTE_EXT;
  info.config_info_set = TRUE;

  //ew->priv->vocore->set_ext_display_info (info);

  return handled;
*/
  return true;
}

static void
ekiga_ext_window_class_init (EkigaExtWindowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructor = constructor;
  object_class->finalize = finalize;

  widget_class->show = show;
  widget_class->draw = draw_event;
  widget_class->focus_in_event = focus_in_event;
}

static void
ekiga_ext_window_init (EkigaExtWindow *ew)
{
  ew->priv = new EkigaExtWindowPrivate;
#ifndef WIN32
  ew->priv->gc = NULL;
#endif
}

GtkWidget *
ext_window_new (boost::shared_ptr<Ekiga::VideoOutputCore> &vocore)
{
  EkigaExtWindow *ew =
    EKIGA_EXT_WINDOW (g_object_new (EKIGA_TYPE_EXT_WINDOW, NULL));

  ew->priv->vocore = vocore;
  ew->priv->video_display_settings =
    boost::shared_ptr<Ekiga::Settings> (new Ekiga::Settings (VIDEO_DISPLAY_SCHEMA));

  g_signal_connect (ew, "delete-event", G_CALLBACK (gtk_true), NULL);
  g_signal_connect (ew->priv->video_display_settings->get_g_settings (),
                    "changed::stay-on-top",
                    G_CALLBACK (stay_on_top_changed_cb), ew);

  ew->priv->vocore->set_ext_display_info (ew->priv->video_stream);

  return GTK_WIDGET (ew);
}

ClutterActor *
ekiga_ext_window_get_stage (EkigaExtWindow *ew)
{
  g_return_val_if_fail (EKIGA_IS_EXT_WINDOW (ew), NULL);

  return ew->priv->stage;
}

ClutterActor *
ekiga_ext_window_get_video_stream (EkigaExtWindow *ew)
{
  g_return_val_if_fail (EKIGA_IS_EXT_WINDOW (ew), NULL);

  return ew->priv->video_stream;
}

void
ekiga_ext_window_set_size (EkigaExtWindow *ew, int width, int height)
{
  int pw, ph;

  g_return_if_fail (width > 0 && height > 0);

  gtk_widget_get_size_request (ew->priv->event_box, &pw, &ph);

  /* No size requisition yet
   * It's our first call so we silently set the new requisition and exit...
   */
  if (pw == -1) {
    gtk_widget_set_size_request (ew->priv->event_box, width, height);
    return;
  }

  /* Do some kind of filtering here. We often get duplicate "size-changed" events...
   * Note that we currently only bother about the width of the event_box.
   */
  if (pw == width)
    return;

  gtk_widget_set_size_request (ew->priv->event_box, width, height);
  gdk_window_invalidate_rect (gtk_widget_get_window (GTK_WIDGET (ew)), NULL, TRUE);
}

void
ekiga_ext_window_destroy (EkigaExtWindow *ew)
{
  clear_display_info (ew);

  /* dirty cheats done dirt cheap: if gtk_widget_destroy it crashes */
  gtk_widget_hide (GTK_WIDGET (ew));
}
