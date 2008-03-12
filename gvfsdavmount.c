
#include <config.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libsoup/soup.h>

/* LibXML2 includes */
#include <libxml/parser.h>
#include <libxml/tree.h>


#define DM_NS "http://purl.org/NET/webdav/mount" 


static void
show_error_dialog (const char *primary, const char *secondary)
{
    GtkWidget *dlg;

    dlg = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
                                  "%s", primary);
    
    if (secondary)
        gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dlg),
                                                  "%s", secondary);

    gtk_dialog_run (GTK_DIALOG (dlg));
    gtk_main_quit ();
}

static void
show_error_dialog_from_error (const char *msg, GError *error)
{
    const char *primary = NULL;
    const char *secondary = NULL;

    if (msg) {
        primary = msg;
        secondary = error->message;
    } else if (error != NULL) {
        primary = error->message;
    } else {
        primary = _("Error mounting WebDAV");
    }

    show_error_dialog (primary, secondary);
    g_error_free (error);
}

/* xml utils */

static inline gboolean
node_has_name_ns (xmlNodePtr node, const char *name, const char *ns_href)
{
  gboolean has_name;
  gboolean has_ns;

  g_return_val_if_fail (node != NULL, FALSE);

  has_name = has_ns = TRUE;

  if (name)
    has_name = node->name && ! strcmp ((char *) node->name, name);

  if (ns_href)
    has_ns = node->ns && node->ns->href &&
      ! g_ascii_strcasecmp ((char *) node->ns->href, ns_href);

  return has_name && has_ns;
}

static inline gboolean
node_is_element (xmlNodePtr node)
{
  return node->type == XML_ELEMENT_NODE && node->name != NULL;
}

static const char *
node_get_content (xmlNodePtr node)
{
    if (node == NULL)
      return NULL;

    switch (node->type)
      {
        case XML_ELEMENT_NODE:
          return node_get_content (node->children);
          break;
        case XML_TEXT_NODE:
          return (const char *) node->content;
          break;
        default:
          return NULL;
      }
}

/* ********* */

static void
mount_done_cb (GObject      *object,
               GAsyncResult *result,
               gpointer      user_data)
{
    GFile    *file;
    gboolean  res;
    GError   *error = NULL;

    file = G_FILE (object);

    res = g_file_mount_enclosing_volume_finish (file,
                                                result,
                                                &error);

  if (res == TRUE) {
      /* spawn nautilus with the file */
  } else {
      show_error_dialog_from_error ("Error during mount", error);
  }

  g_object_unref (file);
  gtk_main_quit ();
}

static void
mount_and_open_file (GFile *file)
{
    GMountOperation *mount_op;

    mount_op = g_mount_operation_new ();

    g_file_mount_enclosing_volume (file, 0, mount_op, NULL,
                                   mount_done_cb, mount_op);
}

static GFile *
parse_file (xmlDocPtr doc, GError **error)
{
    GMountOperation *mount_op;
    const char      *mount_base;
    const char      *target;
    xmlNodePtr       root;
    xmlNodePtr       node;
    GFile           *file;
    char            *new_base;
    char            *uri;


    uri = new_base = NULL;

    if (doc == NULL) {
        return NULL;
    }

    root = xmlDocGetRootElement (doc);

    if (root == NULL || root->children == NULL) {
        goto out;
    }

    if (! node_has_name_ns (root, "mount", DM_NS)) {
        goto out;
    }

    for (node = root->children; node; node = node->next) {

        if (!node_is_element (node))
            continue;

        if (node_has_name_ns (node, "url", DM_NS))
            mount_base = g_strdup (node_get_content (node));
        else if (node_has_name_ns (node, "open", DM_NS))
            target = g_strdup (node_get_content (node));
    }

    if (mount_base == NULL || target == NULL) {
        goto out;
    }

    /* skip the http part (XXX: make sure it really starts
     * with http) */
    mount_base += 4;
    new_base = g_strconcat ("dav", mount_base, NULL);
    uri = g_build_path ("/", new_base, target, NULL);

    file = g_file_new_for_uri (uri);

out:
    g_free (uri);
    g_free (new_base);
    xmlFreeDoc (doc);
    return file;
}

static void
message_ready (SoupSession *session,
               SoupMessage *msg,
               gpointer     user_data)
{
    xmlDocPtr        doc;
    GFile           *file;

    if (!SOUP_STATUS_IS_SUCCESSFUL (msg->status_code)) {
        show_error_dialog (_("HTTP Error"), msg->reason_phrase);
        return;
    }

    doc = xmlReadMemory (msg->response_body->data,
                         msg->response_body->length,
                         "response.xml",
                         NULL,
                         XML_PARSE_NOWARNING |
                         XML_PARSE_NOBLANKS |
                         XML_PARSE_NSCLEAN |
                         XML_PARSE_NOCDATA |
                         XML_PARSE_COMPACT);

    file = parse_file (doc, NULL);
    mount_and_open_file (file);
    g_object_unref (file);
    g_object_unref (session);
}

static void
fetch_file_from_web (const char *url)
{
    SoupSession *session;
    SoupMessage *msg;

    session = soup_session_sync_new ();

    msg = soup_message_new (SOUP_METHOD_GET, url);

    soup_session_queue_message (session, msg,
                                message_ready, NULL);
}

static gboolean
parse_and_open_file (gpointer data)
{
    xmlDocPtr        doc;
    GFile           *file;
    const char      *url;

    url = (const char *) data;

    doc = xmlReadFile (data, NULL,
                       XML_PARSE_NOWARNING |
                       XML_PARSE_NOBLANKS |
                       XML_PARSE_NSCLEAN |
                       XML_PARSE_NOCDATA |
                       XML_PARSE_COMPACT);

    file = parse_file (doc, NULL);
    mount_and_open_file (file);
    g_object_unref (file);
    return FALSE;
}

int
main (int argc, char **argv)
{
    char         *url;
    gboolean      force_rtl = FALSE;
    gboolean      web = FALSE;
    GError       *error = NULL;
    GOptionEntry  options[] = {
        { "web", 'w', 0, G_OPTION_ARG_NONE, &web, "The given location is on a http server", NULL },
        { "right-to-left", 'r', 0, G_OPTION_ARG_NONE, &force_rtl, "Force right-to-left layout.", NULL },
        { NULL }
    };

    g_thread_init (NULL);

    if (!gtk_init_with_args (&argc, &argv, "path", options, NULL, &error)) {
        g_print ("Failed to parse args: %s\n", error->message);
        g_error_free (error);
        return 1;
    }

    if (force_rtl)
        gtk_widget_set_default_direction (GTK_TEXT_DIR_RTL);

    url = argv[1];

    if (web) {
        fetch_file_from_web (url);
    } else {
        g_idle_add (parse_and_open_file, url);
    }

    gtk_main ();

    return 0;
}
