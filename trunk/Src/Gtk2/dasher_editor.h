#ifndef __dasher_editor_h__
#define __dasher_editor_h__

#include <glade/glade.h>
#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include "DasherAppSettings.h"
#include "dasher_action.h"
#include "dasher_buffer_set.h"

/* Forward declaration */
typedef struct _DasherMain DasherMain;
struct _DasherMain;

G_BEGIN_DECLS
#define TYPE_DASHER_EDITOR            (dasher_editor_get_type())
#define DASHER_EDITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_DASHER_EDITOR, DasherEditor ))
#define DASHER_EDITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_DASHER_EDITOR, DasherEditorClass ))
#define IS_DASHER_EDITOR(obj)	      (G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_DASHER_EDITOR))
#define IS_DASHER_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_DASHER_EDITOR))
#define DASHER_EDITOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_DASHER_EDITOR, DasherEditorClass))

typedef struct _DasherEditor DasherEditor;
typedef struct _DasherEditorClass DasherEditorClass;

struct _DasherEditor {
  GtkVBox box;
};

struct _DasherEditorClass {
  GtkVBoxClass parent_class;

  void (*filename_changed)(DasherEditor *pDasherEditor);
  void (*buffer_changed)(DasherEditor *pDasherEditor);
  void (*context_changed)(DasherEditor *pDasherEditor);


  void (*initialise)(DasherEditor *pSelf, DasherAppSettings *pAppSettings, DasherMain *pDasherMain, GladeXML *pGladeXML, const gchar *szFullPath);
  gboolean (*command)(DasherEditor *pSelf, const gchar *szCommand);
  void (*handle_font)(DasherEditor *pSelf, const gchar *szFont);
  void (*output)(DasherEditor *pSelf, const gchar *szText, int iOffset);
  void (*delete_text)(DasherEditor *pSelf, int iLength, int iOffset);
  const gchar *(*get_context)(DasherEditor *pSelf, int iOffset, int iLength);
  gint (*get_offset)(DasherEditor *pSelf);
  void (*handle_stop)(DasherEditor *pSelf);
  void (*handle_start)(DasherEditor *pSelf);
  void (*handle_control)(DasherEditor *pSelf, int iNodeID);
  void (*action_button)(DasherEditor *pSelf, DasherAction *pAction);
  void (*actions_start)(DasherEditor *pSelf);
  bool (*actions_more)(DasherEditor *pSelf);
  void (*actions_get_next)(DasherEditor *pSelf, const gchar **szName, gint *iID, gboolean *bShow, gboolean *bControl, gboolean *bAuto);
  void (*action_set_show)(DasherEditor *pSelf, int iActionID, bool bValue);
  void (*action_set_control)(DasherEditor *pSelf, int iActionID, bool bValue);
  void (*action_set_auto)(DasherEditor *pSelf, int iActionID, bool bValue);
  void (*grab_focus)(DasherEditor *pSelf);
  gboolean (*file_changed)(DasherEditor *pSelf);
  const gchar *(*get_filename)(DasherEditor *pSelf);
  const gchar *(*get_all_text)(DasherEditor *pSelf);
  const gchar *(*get_new_text)(DasherEditor *pSelf);
};

void dasher_editor_initialise(DasherEditor *pSelf, DasherAppSettings *pAppSettings, DasherMain *pDasherMain, GladeXML *pGladeXML, const gchar *szFullPath);

GType dasher_editor_get_type();

/* Replace this with GTK text buffer */
//IDasherBufferSet *dasher_editor_get_buffer_set(DasherEditor *pSelf);

// TODO: Just have one 'handle event' method?
gboolean dasher_editor_command(DasherEditor *pSelf, const gchar *szCommand);

void dasher_editor_handle_font(DasherEditor *pSelf, const gchar *szFont);

/* To be obsoleted by movement to GTK buffers */
void dasher_editor_output(DasherEditor *pSelf, const gchar *szText, int iOffset);
void dasher_editor_delete(DasherEditor *pSelf, int iLength, int iOffset);
const gchar *dasher_editor_get_context(DasherEditor *pSelf, int iOffset, int iLength);
gint dasher_editor_get_offset(DasherEditor *pSelf);

/* Events proagated from main */
void dasher_editor_handle_stop(DasherEditor *pSelf);
void dasher_editor_handle_start(DasherEditor *pSelf);
void dasher_editor_handle_control(DasherEditor *pSelf, int iNodeID);

/* Action related methods - TODO: a lot of this should be moved to dasher_main (eg action on stop etc) - that way we get a better level of abstraction, and can incorporate commands from otehr modules too. Actions should only be externally visible as a list of string commands*/
void dasher_editor_action_button(DasherEditor *pSelf, DasherAction *pAction);
void dasher_editor_actions_start(DasherEditor *pSelf);
bool dasher_editor_actions_more(DasherEditor *pSelf);
void dasher_editor_actions_get_next(DasherEditor *pSelf, const gchar **szName, gint *iID, gboolean *bShow, gboolean *bControl, gboolean *bAuto);
void dasher_editor_action_set_show(DasherEditor *pSelf, int iActionID, bool bValue);
void dasher_editor_action_set_control(DasherEditor *pSelf, int iActionID, bool bValue);
void dasher_editor_action_set_auto(DasherEditor *pSelf, int iActionID, bool bValue);

void dasher_editor_grab_focus(DasherEditor *pSelf);

/* TODO: Tutorial editor should be a separate class */
//void dasher_editor_start_tutorial(DasherEditor *pSelf);

/* Todo: possibly tidy up the need to have this public (quit in dasher_main possibly too connected) */
gboolean dasher_editor_file_changed(DasherEditor *pSelf);
const gchar *dasher_editor_get_filename(DasherEditor *pSelf);

const gchar *dasher_editor_get_all_text(DasherEditor *pSelf);
const gchar *dasher_editor_get_new_text(DasherEditor *pSelf);
G_END_DECLS

#endif
