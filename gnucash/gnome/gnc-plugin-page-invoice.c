/*
 * gnc-plugin-page-invoice.c --
 *
 * Copyright (C) 2005,2006 David Hampton <hampton@employees.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, contact:
 *
 * Free Software Foundation           Voice:  +1-617-542-5942
 * 51 Franklin Street, Fifth Floor    Fax:    +1-617-542-2652
 * Boston, MA  02110-1301,  USA       gnu@gnu.org
 */

#include <config.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "gnc-plugin.h"
#include "business-gnome-utils.h"
#include "dialog-invoice.h"
#include "gnc-ledger-display.h"
#include "gnc-plugin-page-invoice.h"

#include "dialog-account.h"
#include "gnc-component-manager.h"
#include "gnc-gobject-utils.h"
#include "gnc-gnome-utils.h"
#include "gnc-icons.h"
#include "gnucash-register.h"
#include "gnc-prefs.h"
#include "gnc-ui-util.h"
#include "gnc-window.h"
#include "dialog-utils.h"

/* This static indicates the debugging module that this .o belongs to.  */
static QofLogModule log_module = GNC_MOD_GUI;

static void gnc_plugin_page_invoice_class_init (GncPluginPageInvoiceClass *klass);
static void gnc_plugin_page_invoice_init (GncPluginPageInvoice *plugin_page);
static void gnc_plugin_page_invoice_finalize (GObject *object);

static GtkWidget *gnc_plugin_page_invoice_create_widget (GncPluginPage *plugin_page);
static void gnc_plugin_page_invoice_destroy_widget (GncPluginPage *plugin_page);
static void gnc_plugin_page_invoice_save_page (GncPluginPage *plugin_page, GKeyFile *file, const gchar *group);
static GncPluginPage *gnc_plugin_page_invoice_recreate_page (GtkWidget *window, GKeyFile *file, const gchar *group);
static void gnc_plugin_page_invoice_window_changed (GncPluginPage *plugin_page, GtkWidget *window);

static void gnc_plugin_page_invoice_summarybar_position_changed(gpointer prefs, gchar* pref, gpointer user_data);

/* Command callbacks */
static void gnc_plugin_page_invoice_cmd_new_invoice (GtkAction *action, GncPluginPageInvoice *plugin_page);
static void gnc_plugin_page_invoice_cmd_new_account (GtkAction *action, GncPluginPageInvoice *plugin_page);
static void gnc_plugin_page_invoice_cmd_print (GtkAction *action, GncPluginPageInvoice *plugin_page);
static void gnc_plugin_page_invoice_cmd_cut (GtkAction *action, GncPluginPageInvoice *plugin_page);
static void gnc_plugin_page_invoice_cmd_copy (GtkAction *action, GncPluginPageInvoice *plugin_page);
static void gnc_plugin_page_invoice_cmd_paste (GtkAction *action, GncPluginPageInvoice *plugin_page);
static void gnc_plugin_page_invoice_cmd_edit (GtkAction *action, GncPluginPageInvoice *plugin_page);
static void gnc_plugin_page_invoice_cmd_duplicateInvoice (GtkAction *action, GncPluginPageInvoice *plugin_page);
static void gnc_plugin_page_invoice_cmd_post (GtkAction *action, GncPluginPageInvoice *plugin_page);
static void gnc_plugin_page_invoice_cmd_unpost (GtkAction *action, GncPluginPageInvoice *plugin_page);

static void gnc_plugin_page_invoice_cmd_sort_changed (GtkAction *action,
        GtkRadioAction *current,
        GncPluginPageInvoice *plugin_page);

static void gnc_plugin_page_invoice_cmd_enter (GtkAction *action, GncPluginPageInvoice *plugin_page);
static void gnc_plugin_page_invoice_cmd_cancel (GtkAction *action, GncPluginPageInvoice *plugin_page);
static void gnc_plugin_page_invoice_cmd_delete (GtkAction *action, GncPluginPageInvoice *plugin_page);
static void gnc_plugin_page_invoice_cmd_blank (GtkAction *action, GncPluginPageInvoice *plugin_page);
static void gnc_plugin_page_invoice_cmd_duplicateEntry (GtkAction *action, GncPluginPageInvoice *plugin_page);
static void gnc_plugin_page_invoice_cmd_pay_invoice (GtkAction *action, GncPluginPageInvoice *plugin_page);
static void gnc_plugin_page_invoice_cmd_company_report (GtkAction *action, GncPluginPageInvoice *plugin_page);

static void gnc_plugin_page_redraw_help_cb( GnucashRegister *gsr, GncPluginPageInvoice *invoice_page );
static void gnc_plugin_page_invoice_refresh_cb (GHashTable *changes, gpointer user_data);

static void gnc_plugin_page_invoice_cmd_entryUp (GtkAction *action, GncPluginPageInvoice *plugin_page);
static void gnc_plugin_page_invoice_cmd_entryDown (GtkAction *action, GncPluginPageInvoice *plugin_page);

/************************************************************
 *                          Actions                         *
 ************************************************************/
// FIXME:  The texts are wrong if we have a Bill or Expence Voucher.
static GtkActionEntry gnc_plugin_page_invoice_actions [] =
{
    /* Toplevel */
    { "FakeToplevel", NULL, "", NULL, NULL, NULL },
    { "SortOrderAction", NULL, N_("Sort _Order"), NULL, NULL, NULL },

    /* File menu */
    {
        "FileNewAccountAction", GNC_ICON_NEW_ACCOUNT, N_("New _Account..."), NULL,
        N_("Create a new account"),
        G_CALLBACK (gnc_plugin_page_invoice_cmd_new_account)
    },
    {
        "FilePrintAction", "document-print", N_("Print Invoice"), "<primary>p",
        N_("Make a printable invoice"),
        G_CALLBACK (gnc_plugin_page_invoice_cmd_print)
    },

    /* Edit menu */
    {
        "EditCutAction", "edit-cut", N_("_Cut"), "<primary>X",
        NULL,
        G_CALLBACK (gnc_plugin_page_invoice_cmd_cut)
    },
    {
        "EditCopyAction", "edit-copy", N_("Copy"), "<primary>C",
        NULL,
        G_CALLBACK (gnc_plugin_page_invoice_cmd_copy)
    },
    {
        "EditPasteAction", "edit-paste", N_("_Paste"), "<primary>V",
        NULL,
        G_CALLBACK (gnc_plugin_page_invoice_cmd_paste)
    },
    {
        "EditEditInvoiceAction", GNC_ICON_INVOICE_EDIT, N_("_Edit Invoice"), NULL,
        N_("Edit this invoice"),
        G_CALLBACK (gnc_plugin_page_invoice_cmd_edit)
    },
    {
        "EditDuplicateInvoiceAction", GNC_ICON_INVOICE_DUPLICATE, N_("_Duplicate Invoice"),
        NULL, N_("Create a new invoice as a duplicate of the current one"),
        G_CALLBACK (gnc_plugin_page_invoice_cmd_duplicateInvoice)
    },
    {
        "EditPostInvoiceAction", GNC_ICON_INVOICE_POST, N_("_Post Invoice"), NULL,
        N_("Post this Invoice to your Chart of Accounts"),
        G_CALLBACK (gnc_plugin_page_invoice_cmd_post)
    },
    {
        "EditUnpostInvoiceAction", GNC_ICON_INVOICE_UNPOST, N_("_Unpost Invoice"), NULL,
        N_("Unpost this Invoice and make it editable"),
        G_CALLBACK (gnc_plugin_page_invoice_cmd_unpost)
    },

    /* Actions menu */
    {
        "RecordEntryAction", "list-add", N_("_Enter"), NULL,
        N_("Record the current entry"),
        G_CALLBACK (gnc_plugin_page_invoice_cmd_enter)
    },
    {
        "CancelEntryAction", "process-stop", N_("_Cancel"), NULL,
        N_("Cancel the current entry"),
        G_CALLBACK (gnc_plugin_page_invoice_cmd_cancel)
    },
    {
        "DeleteEntryAction", "edit-delete", N_("_Delete"), NULL,
        N_("Delete the current entry"),
        G_CALLBACK (gnc_plugin_page_invoice_cmd_delete)
    },
    {
        "BlankEntryAction", "go-bottom", N_("_Blank"), NULL,
        N_("Move to the blank entry at the bottom of the Invoice"),
        G_CALLBACK (gnc_plugin_page_invoice_cmd_blank)
    },
    {
        "DuplicateEntryAction", "edit-copy", N_("Dup_licate Entry"), NULL,
        N_("Make a copy of the current entry"),
        G_CALLBACK (gnc_plugin_page_invoice_cmd_duplicateEntry)
    },
    {
        "EntryUpAction", "go-up", N_("Move Entry _Up"), NULL,
        N_("Move the current entry one row upwards"),
        G_CALLBACK (gnc_plugin_page_invoice_cmd_entryUp)
    },
    {
        "EntryDownAction", "go-down", N_("Move Entry Do_wn"), NULL,
        N_("Move the current entry one row downwards"),
        G_CALLBACK (gnc_plugin_page_invoice_cmd_entryDown)
    },

    /* Business menu */
    {
        "BusinessNewInvoiceAction", GNC_ICON_INVOICE_NEW, N_("New _Invoice"), "",
        N_("Create a new invoice for the same owner as the current one"),
        G_CALLBACK (gnc_plugin_page_invoice_cmd_new_invoice)
    },
    {
        "ToolsProcessPaymentAction", GNC_ICON_INVOICE_PAY, N_("_Pay Invoice"), NULL,
        N_("Enter a payment for the owner of this Invoice"),
        G_CALLBACK (gnc_plugin_page_invoice_cmd_pay_invoice)
    },

    /* Reports menu */
    {
        "ReportsCompanyReportAction", NULL, N_("_Company Report"), NULL,
        N_("Open a company report window for the owner of this Invoice"),
        G_CALLBACK (gnc_plugin_page_invoice_cmd_company_report)
    },
};
static guint gnc_plugin_page_invoice_n_actions = G_N_ELEMENTS (gnc_plugin_page_invoice_actions);

static GtkRadioActionEntry radio_entries [] =
{
    { "SortStandardAction", NULL, N_("_Standard"), NULL, N_("Keep normal invoice order"), INVSORT_BY_STANDARD },
    { "SortDateAction", NULL, N_("_Date"), NULL, N_("Sort by date"), INVSORT_BY_DATE },
    { "SortDateEntryAction", NULL, N_("Date of _Entry"), NULL, N_("Sort by the date of entry"), INVSORT_BY_DATE_ENTERED },
    { "SortQuantityAction", NULL, N_("_Quantity"), NULL, N_("Sort by quantity"), INVSORT_BY_QTY },
    { "SortPriceAction", NULL, N_("_Price"), NULL, N_("Sort by price"), INVSORT_BY_PRICE },
    { "SortDescriptionAction", NULL, N_("Descri_ption"), NULL, N_("Sort by description"), INVSORT_BY_DESC },
};
static guint n_radio_entries = G_N_ELEMENTS (radio_entries);

static const gchar *invoice_book_readwrite_actions[] =
{
    // Only insert actions here which are not yet in posted_actions and unposted_actions!
    "FileNewAccountAction",
    "EditDuplicateInvoiceAction",
    "BusinessNewInvoiceAction",
    "ToolsProcessPaymentAction",
    NULL
};

static const gchar *posted_actions[] =
{
    NULL
};

static const gchar *unposted_actions[] =
{
    "EditCutAction",
    "EditPasteAction",
    "EditEditInvoiceAction",
    "EditPostInvoiceAction",
    "RecordEntryAction",
    "CancelEntryAction",
    "DeleteEntryAction",
    "DuplicateEntryAction",
    "EntryUpAction",
    "EntryDownAction",
    "BlankEntryAction",
    NULL
};

static const gchar *can_unpost_actions[] =
{
    "EditUnpostInvoiceAction",
    NULL
};

/** Short labels for use on the toolbar buttons. */
static action_toolbar_labels toolbar_labels[] =
{
    { "RecordEntryAction",    N_("Enter") },
    { "CancelEntryAction",    N_("Cancel") },
    { "DeleteEntryAction",    N_("Delete") },
    { "DuplicateEntryAction", N_("Duplicate") },
    { "EntryUpAction",        N_("Up") },
    { "EntryDownAction",      N_("Down") },
    { "BlankEntryAction",           N_("Blank") },
    { "EditPostInvoiceAction",      N_("Post") },
    { "EditUnpostInvoiceAction",    N_("Unpost") },
    { "ToolsProcessPaymentAction",    N_("Pay") },
    { NULL, NULL },
};


/************************************************************/
/*                      Data Structures                     */
/************************************************************/

typedef struct GncPluginPageInvoicePrivate
{
    InvoiceWindow *iw;

    GtkWidget *widget;

    gint component_manager_id;
} GncPluginPageInvoicePrivate;

G_DEFINE_TYPE_WITH_PRIVATE(GncPluginPageInvoice, gnc_plugin_page_invoice, GNC_TYPE_PLUGIN_PAGE)

#define GNC_PLUGIN_PAGE_INVOICE_GET_PRIVATE(o)  \
   (G_TYPE_INSTANCE_GET_PRIVATE ((o), GNC_TYPE_PLUGIN_PAGE_INVOICE, GncPluginPageInvoicePrivate))

static GObjectClass *parent_class = NULL;

/************************************************************/
/*                      Implementation                      */
/************************************************************/

GncPluginPage *
gnc_plugin_page_invoice_new (InvoiceWindow *iw)
{
    GncPluginPageInvoicePrivate *priv;
    GncPluginPageInvoice *invoice_page;
    GncPluginPage *plugin_page;
    const GList *item;

    /* Is there an existing page? */
    item = gnc_gobject_tracking_get_list(GNC_PLUGIN_PAGE_INVOICE_NAME);
    for ( ; item; item = g_list_next(item))
    {
        invoice_page = (GncPluginPageInvoice *)item->data;
        priv = GNC_PLUGIN_PAGE_INVOICE_GET_PRIVATE(invoice_page);
        if (priv->iw == iw)
            return GNC_PLUGIN_PAGE(invoice_page);
    }

    invoice_page = g_object_new (GNC_TYPE_PLUGIN_PAGE_INVOICE, (char *)NULL);
    priv = GNC_PLUGIN_PAGE_INVOICE_GET_PRIVATE(invoice_page);
    priv->iw = iw;

    plugin_page = GNC_PLUGIN_PAGE(invoice_page);
    gnc_plugin_page_invoice_update_title(plugin_page);
    gnc_plugin_page_set_uri(plugin_page, "default:");

    priv->component_manager_id = 0;
    return plugin_page;
}

static void
gnc_plugin_page_invoice_class_init (GncPluginPageInvoiceClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GncPluginPageClass *gnc_plugin_class = GNC_PLUGIN_PAGE_CLASS(klass);

    parent_class = g_type_class_peek_parent (klass);

    object_class->finalize = gnc_plugin_page_invoice_finalize;

    gnc_plugin_class->tab_icon        = NULL;
    gnc_plugin_class->plugin_name     = GNC_PLUGIN_PAGE_INVOICE_NAME;
    gnc_plugin_class->create_widget   = gnc_plugin_page_invoice_create_widget;
    gnc_plugin_class->destroy_widget  = gnc_plugin_page_invoice_destroy_widget;
    gnc_plugin_class->save_page       = gnc_plugin_page_invoice_save_page;
    gnc_plugin_class->recreate_page   = gnc_plugin_page_invoice_recreate_page;
    gnc_plugin_class->window_changed  = gnc_plugin_page_invoice_window_changed;
}

static void
gnc_plugin_page_invoice_init (GncPluginPageInvoice *plugin_page)
{
    GncPluginPage *parent;
    GtkActionGroup *action_group;
    gboolean use_new;

    /* Init parent declared variables */
    parent = GNC_PLUGIN_PAGE(plugin_page);
    use_new = gnc_prefs_get_bool (GNC_PREFS_GROUP_INVOICE, GNC_PREF_USE_NEW);
    g_object_set(G_OBJECT(plugin_page),
                 "page-name",      _("Invoice"),
                 "page-uri",       "default:",
                 "ui-description", "gnc-plugin-page-invoice-ui.xml",
                 "use-new-window", use_new,
                 (char *)NULL);

    /* change me when the system supports multiple books */
    gnc_plugin_page_add_book(parent, gnc_get_current_book());

    /* Create menu and toolbar information */
    action_group =
        gnc_plugin_page_create_action_group(parent,
                                            "GncPluginPageInvoiceActions");
    gtk_action_group_add_actions (action_group, gnc_plugin_page_invoice_actions,
                                  gnc_plugin_page_invoice_n_actions, plugin_page);
    gtk_action_group_add_radio_actions (action_group,
                                        radio_entries, n_radio_entries,
                                        REG_STYLE_LEDGER,
                                        G_CALLBACK(gnc_plugin_page_invoice_cmd_sort_changed),
                                        plugin_page);

    gnc_plugin_init_short_names (action_group, toolbar_labels);
}

static void
gnc_plugin_page_invoice_finalize (GObject *object)
{
    g_return_if_fail (GNC_IS_PLUGIN_PAGE_INVOICE (object));

    ENTER("object %p", object);
    G_OBJECT_CLASS (parent_class)->finalize (object);
    LEAVE(" ");
}


void
gnc_plugin_page_invoice_update_menus (GncPluginPage *page, gboolean is_posted, gboolean can_unpost)
{
    GtkActionGroup *action_group;
    gboolean is_readonly = qof_book_is_readonly(gnc_get_current_book());

    g_return_if_fail(GNC_IS_PLUGIN_PAGE_INVOICE(page));

    if (is_readonly)
    {
        // Are we readonly? Then don't allow any actions.
        is_posted = TRUE;
        can_unpost = FALSE;
    }

    action_group = gnc_plugin_page_get_action_group(page);
    gnc_plugin_update_actions (action_group, posted_actions,
                               "sensitive", is_posted);
    gnc_plugin_update_actions (action_group, unposted_actions,
                               "sensitive", !is_posted);
    gnc_plugin_update_actions (action_group, can_unpost_actions,
                               "sensitive", can_unpost);
    gnc_plugin_update_actions (action_group, invoice_book_readwrite_actions,
                               "sensitive", !is_readonly);
}


static gboolean
gnc_plugin_page_invoice_focus (InvoiceWindow *iw)
{
    GtkWidget *regWidget = gnc_invoice_get_register(iw);
    GtkWidget *notes = gnc_invoice_get_notes(iw);
    GnucashSheet *sheet;

    if (!GNUCASH_IS_REGISTER(regWidget))
        return FALSE;

    sheet = gnucash_register_get_sheet (GNUCASH_REGISTER(regWidget));

    // Test for the sheet being read only
    if (!gnucash_sheet_is_read_only (sheet))
    {
        if (!gtk_widget_is_focus (GTK_WIDGET(sheet)))
            gtk_widget_grab_focus (GTK_WIDGET(sheet));
    }
    else // set focus to the notes field
    {
        if (!gtk_widget_is_focus (GTK_WIDGET(notes)))
            gtk_widget_grab_focus (GTK_WIDGET(notes));
    }
    return FALSE;
}


/**
 * Whenever the current page is changed, if an invoice page is
 * the current page, set focus on the sheet or notes field.
 */
static void
gnc_plugin_page_invoice_main_window_page_changed (GncMainWindow *window,
        GncPluginPage *plugin_page, gpointer user_data)
{
    // We continue only if the plugin_page is a valid
    if (!plugin_page || !GNC_IS_PLUGIN_PAGE(plugin_page))
        return;

    if (gnc_main_window_get_current_page (window) == plugin_page)
    {
        GncPluginPageInvoice *page;
        GncPluginPageInvoicePrivate *priv;

        if (!GNC_IS_PLUGIN_PAGE_INVOICE(plugin_page))
            return;

        page = GNC_PLUGIN_PAGE_INVOICE(plugin_page);
        priv = GNC_PLUGIN_PAGE_INVOICE_GET_PRIVATE(page);

        // The page changed signal is emitted multiple times so we need
        // to use an idle_add to change the focus to the sheet
        g_idle_remove_by_data (priv->iw);
        g_idle_add ((GSourceFunc)gnc_plugin_page_invoice_focus, priv->iw);
    }
}


/* Virtual Functions */

static GtkWidget *
gnc_plugin_page_invoice_create_widget (GncPluginPage *plugin_page)
{
    GncPluginPageInvoice *page;
    GncPluginPageInvoicePrivate *priv;
    GtkWidget *regWidget, *widget;
    GncMainWindow  *window;

    ENTER("page %p", plugin_page);
    page = GNC_PLUGIN_PAGE_INVOICE (plugin_page);
    priv = GNC_PLUGIN_PAGE_INVOICE_GET_PRIVATE(page);
    if (priv->widget != NULL)
    {
        LEAVE("");
        return priv->widget;
    }

    priv->widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_set_homogeneous (GTK_BOX (priv->widget), FALSE);

    // Set the style context for this page so it can be easily manipulated with css
    gnc_widget_set_style_context (GTK_WIDGET(priv->widget), "GncInvoicePage");

    gtk_widget_show (priv->widget);

    widget = gnc_invoice_create_page(priv->iw, page);
    gtk_widget_show (widget);
    gtk_box_pack_start(GTK_BOX (priv->widget), widget, TRUE, TRUE, 0);

    plugin_page->summarybar = gnc_invoice_window_create_summary_bar(priv->iw);
    gtk_box_pack_start(GTK_BOX (priv->widget), plugin_page->summarybar, FALSE, FALSE, 0);
    gnc_plugin_page_invoice_summarybar_position_changed(NULL, NULL, page);
    gnc_prefs_register_cb (GNC_PREFS_GROUP_GENERAL,
                           GNC_PREF_SUMMARYBAR_POSITION_TOP,
                           gnc_plugin_page_invoice_summarybar_position_changed,
                           page);
    gnc_prefs_register_cb (GNC_PREFS_GROUP_GENERAL,
                           GNC_PREF_SUMMARYBAR_POSITION_BOTTOM,
                           gnc_plugin_page_invoice_summarybar_position_changed,
                           page);

    regWidget = gnc_invoice_get_register(priv->iw);
    if (regWidget)
    {
        g_signal_connect (G_OBJECT (regWidget), "redraw-help",
                          G_CALLBACK (gnc_plugin_page_redraw_help_cb), page);
    }

    priv->component_manager_id =
        gnc_register_gui_component(GNC_PLUGIN_PAGE_INVOICE_NAME,
                                   gnc_plugin_page_invoice_refresh_cb,
                                   NULL, page);

    window = GNC_MAIN_WINDOW(GNC_PLUGIN_PAGE(plugin_page)->window);
    g_signal_connect(window, "page_changed",
                     G_CALLBACK(gnc_plugin_page_invoice_main_window_page_changed),
                     plugin_page);

    LEAVE("");
    return priv->widget;
}

static void
gnc_plugin_page_invoice_destroy_widget (GncPluginPage *plugin_page)
{
    GncPluginPageInvoice *page;
    GncPluginPageInvoicePrivate *priv;

    ENTER("page %p", plugin_page);
    page = GNC_PLUGIN_PAGE_INVOICE (plugin_page);
    priv = GNC_PLUGIN_PAGE_INVOICE_GET_PRIVATE(page);

    gnc_prefs_remove_cb_by_func (GNC_PREFS_GROUP_GENERAL,
                                 GNC_PREF_SUMMARYBAR_POSITION_TOP,
                                 gnc_plugin_page_invoice_summarybar_position_changed,
                                 page);
    gnc_prefs_remove_cb_by_func (GNC_PREFS_GROUP_GENERAL,
                                 GNC_PREF_SUMMARYBAR_POSITION_BOTTOM,
                                 gnc_plugin_page_invoice_summarybar_position_changed,
                                 page);

    // Remove the page focus idle function if present
    g_idle_remove_by_data (priv->iw);

    if (priv->widget == NULL)
    {
        LEAVE("");
        return;
    }

    if (priv->component_manager_id)
    {
        gnc_unregister_gui_component(priv->component_manager_id);
        priv->component_manager_id = 0;
    }

    gtk_widget_hide(priv->widget);
    gnc_invoice_window_destroy_cb(priv->widget, priv->iw);
    priv->widget = NULL;
    LEAVE("");
}

/** Save enough information about this invoice page that it can be
 *  recreated next time the user starts gnucash.
 *
 *  @param page The page to save.
 *
 *  @param key_file A pointer to the GKeyFile data structure where the
 *  page information should be written.
 *
 *  @param group_name The group name to use when saving data. */
static void
gnc_plugin_page_invoice_save_page (GncPluginPage *plugin_page,
                                   GKeyFile *key_file,
                                   const gchar *group_name)
{
    GncPluginPageInvoice *invoice;
    GncPluginPageInvoicePrivate *priv;

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_INVOICE(plugin_page));
    g_return_if_fail (key_file != NULL);
    g_return_if_fail (group_name != NULL);

    ENTER("page %p, key_file %p, group_name %s", plugin_page, key_file,
          group_name);

    invoice = GNC_PLUGIN_PAGE_INVOICE(plugin_page);
    priv = GNC_PLUGIN_PAGE_INVOICE_GET_PRIVATE(invoice);

    gnc_invoice_save_page(priv->iw, key_file, group_name);
    LEAVE(" ");
}



/** Create a new invoice page based on the information saved during a
 *  previous instantiation of gnucash.
 *
 *  @param window The window where this page should be installed.
 *
 *  @param key_file A pointer to the GKeyFile data structure where the
 *  page information should be read.
 *
 *  @param group_name The group name to use when restoring data. */
static GncPluginPage *
gnc_plugin_page_invoice_recreate_page (GtkWidget *window,
                                       GKeyFile *key_file,
                                       const gchar *group_name)
{
    GncPluginPage *page;

    g_return_val_if_fail(GNC_IS_MAIN_WINDOW(window), NULL);
    g_return_val_if_fail(key_file, NULL);
    g_return_val_if_fail(group_name, NULL);
    ENTER("key_file %p, group_name %s", key_file, group_name);

    /* Create the new page. */
    page = gnc_invoice_recreate_page(GNC_MAIN_WINDOW(window),
                                     key_file, group_name);

    LEAVE(" ");
    return page;
}


static void
gnc_plugin_page_invoice_window_changed (GncPluginPage *plugin_page,
                                        GtkWidget *window)
{
    GncPluginPageInvoice *page;
    GncPluginPageInvoicePrivate *priv;

    g_return_if_fail (GNC_IS_PLUGIN_PAGE_INVOICE (plugin_page));

    page = GNC_PLUGIN_PAGE_INVOICE(plugin_page);
    priv = GNC_PLUGIN_PAGE_INVOICE_GET_PRIVATE(page);
    gnc_invoice_window_changed (priv->iw, window);
}


static void
gnc_plugin_page_invoice_summarybar_position_changed(gpointer prefs, gchar *pref, gpointer user_data)
{
    GncPluginPage *plugin_page;
    GncPluginPageInvoice *page;
    GncPluginPageInvoicePrivate *priv;
    GtkPositionType position = GTK_POS_BOTTOM;

    g_return_if_fail(user_data != NULL);

    plugin_page = GNC_PLUGIN_PAGE(user_data);
    page = GNC_PLUGIN_PAGE_INVOICE (user_data);
    priv = GNC_PLUGIN_PAGE_INVOICE_GET_PRIVATE(page);

    if (gnc_prefs_get_bool (GNC_PREFS_GROUP_GENERAL, GNC_PREF_SUMMARYBAR_POSITION_TOP))
        position = GTK_POS_TOP;

    gtk_box_reorder_child(GTK_BOX(priv->widget),
                          plugin_page->summarybar,
                          (position == GTK_POS_TOP ? 0 : -1) );
}


/************************************************************/
/*                     Command callbacks                    */
/************************************************************/

static void
gnc_plugin_page_invoice_cmd_new_invoice (GtkAction *action,
        GncPluginPageInvoice *plugin_page)
{
    GncPluginPageInvoicePrivate *priv;
    GtkWindow *parent;

    g_return_if_fail(GNC_IS_PLUGIN_PAGE_INVOICE(plugin_page));

    ENTER("(action %p, plugin_page %p)", action, plugin_page);
    priv = GNC_PLUGIN_PAGE_INVOICE_GET_PRIVATE(plugin_page);
    parent = GTK_WINDOW (gnc_plugin_page_get_window (GNC_PLUGIN_PAGE (plugin_page)));
    gnc_invoice_window_new_invoice_cb(parent, priv->iw);
    LEAVE(" ");
}

static void
gnc_plugin_page_invoice_cmd_new_account (GtkAction *action,
        GncPluginPageInvoice *plugin_page)
{
    GtkWindow *parent = NULL;
    g_return_if_fail(GNC_IS_PLUGIN_PAGE_INVOICE(plugin_page));
    parent = GTK_WINDOW (gnc_plugin_page_get_window (GNC_PLUGIN_PAGE (plugin_page)));
    ENTER("(action %p, plugin_page %p)", action, plugin_page);
    gnc_ui_new_account_window (parent, gnc_get_current_book(), NULL);
    LEAVE(" ");
}

static void
gnc_plugin_page_invoice_cmd_print (GtkAction *action,
                                   GncPluginPageInvoice *plugin_page)
{
    GncPluginPageInvoicePrivate *priv;
    GtkWindow *parent;

    g_return_if_fail(GNC_IS_PLUGIN_PAGE_INVOICE(plugin_page));

    ENTER("(action %p, plugin_page %p)", action, plugin_page);
    priv = GNC_PLUGIN_PAGE_INVOICE_GET_PRIVATE(plugin_page);
    parent = GTK_WINDOW (gnc_plugin_page_get_window (GNC_PLUGIN_PAGE (plugin_page)));
    gnc_invoice_window_printCB (parent, priv->iw);
    LEAVE(" ");
}

static void
gnc_plugin_page_invoice_cmd_cut (GtkAction *action,
                                 GncPluginPageInvoice *plugin_page)
{
    GncPluginPageInvoicePrivate *priv;

    g_return_if_fail(GNC_IS_PLUGIN_PAGE_INVOICE(plugin_page));

    ENTER("(action %p, plugin_page %p)", action, plugin_page);
    priv = GNC_PLUGIN_PAGE_INVOICE_GET_PRIVATE(plugin_page);
    gnc_invoice_window_cut_cb(NULL, priv->iw);
    LEAVE(" ");
}

static void
gnc_plugin_page_invoice_cmd_copy (GtkAction *action,
                                  GncPluginPageInvoice *plugin_page)
{
    GncPluginPageInvoicePrivate *priv;

    g_return_if_fail(GNC_IS_PLUGIN_PAGE_INVOICE(plugin_page));

    ENTER("(action %p, plugin_page %p)", action, plugin_page);
    priv = GNC_PLUGIN_PAGE_INVOICE_GET_PRIVATE(plugin_page);
    gnc_invoice_window_copy_cb(NULL, priv->iw);
    LEAVE(" ");
}

static void
gnc_plugin_page_invoice_cmd_paste (GtkAction *action,
                                   GncPluginPageInvoice *plugin_page)
{
    GncPluginPageInvoicePrivate *priv;

    g_return_if_fail(GNC_IS_PLUGIN_PAGE_INVOICE(plugin_page));

    ENTER("(action %p, plugin_page %p)", action, plugin_page);
    priv = GNC_PLUGIN_PAGE_INVOICE_GET_PRIVATE(plugin_page);
    gnc_invoice_window_paste_cb(NULL, priv->iw);
    LEAVE(" ");
}

static void
gnc_plugin_page_invoice_cmd_edit (GtkAction *action,
                                  GncPluginPageInvoice *plugin_page)
{
    GncPluginPageInvoicePrivate *priv;
    GtkWindow *parent;

    g_return_if_fail(GNC_IS_PLUGIN_PAGE_INVOICE(plugin_page));

    ENTER("(action %p, plugin_page %p)", action, plugin_page);
    priv = GNC_PLUGIN_PAGE_INVOICE_GET_PRIVATE(plugin_page);
    parent = GTK_WINDOW (gnc_plugin_page_get_window (GNC_PLUGIN_PAGE (plugin_page)));
    gnc_invoice_window_editCB (parent, priv->iw);
    LEAVE(" ");
}

static void
gnc_plugin_page_invoice_cmd_duplicateInvoice (GtkAction *action,
        GncPluginPageInvoice *plugin_page)
{
    GncPluginPageInvoicePrivate *priv;
    GtkWindow *parent;

    g_return_if_fail(GNC_IS_PLUGIN_PAGE_INVOICE(plugin_page));

    ENTER("(action %p, plugin_page %p)", action, plugin_page);
    priv = GNC_PLUGIN_PAGE_INVOICE_GET_PRIVATE(plugin_page);
    parent = GTK_WINDOW (gnc_plugin_page_get_window (GNC_PLUGIN_PAGE (plugin_page)));
    gnc_invoice_window_duplicateInvoiceCB(parent, priv->iw);
    LEAVE(" ");
}

static void
gnc_plugin_page_invoice_cmd_post (GtkAction *action,
                                  GncPluginPageInvoice *plugin_page)
{
    GncPluginPageInvoicePrivate *priv;

    g_return_if_fail(GNC_IS_PLUGIN_PAGE_INVOICE(plugin_page));

    ENTER("(action %p, plugin_page %p)", action, plugin_page);
    priv = GNC_PLUGIN_PAGE_INVOICE_GET_PRIVATE(plugin_page);
    gnc_invoice_window_postCB(NULL, priv->iw);
    LEAVE(" ");
}

static void
gnc_plugin_page_invoice_cmd_unpost (GtkAction *action,
                                    GncPluginPageInvoice *plugin_page)
{
    GncPluginPageInvoicePrivate *priv;

    g_return_if_fail(GNC_IS_PLUGIN_PAGE_INVOICE(plugin_page));

    ENTER("(action %p, plugin_page %p)", action, plugin_page);
    priv = GNC_PLUGIN_PAGE_INVOICE_GET_PRIVATE(plugin_page);
    gnc_invoice_window_unpostCB(NULL, priv->iw);
    LEAVE(" ");
}

static void
gnc_plugin_page_invoice_cmd_sort_changed (GtkAction *action,
        GtkRadioAction *current,
        GncPluginPageInvoice *plugin_page)
{
    GncPluginPageInvoicePrivate *priv;
    invoice_sort_type_t value;

    ENTER("(action %p, radio action %p, plugin_page %p)",
          action, current, plugin_page);
    LEAVE("g_return testing...");

    g_return_if_fail(GTK_IS_ACTION(action));
    g_return_if_fail(GTK_IS_RADIO_ACTION(current));
    g_return_if_fail(GNC_IS_PLUGIN_PAGE_INVOICE(plugin_page));

    ENTER("...passed (action %p, radio action %p, plugin_page %p)",
          action, current, plugin_page);
    priv = GNC_PLUGIN_PAGE_INVOICE_GET_PRIVATE(plugin_page);
    value = gtk_radio_action_get_current_value(current);
    gnc_invoice_window_sort (priv->iw, value);
    LEAVE(" ");
}


static void
gnc_plugin_page_invoice_cmd_enter (GtkAction *action,
                                   GncPluginPageInvoice *plugin_page)
{
    GncPluginPageInvoicePrivate *priv;

    g_return_if_fail(GNC_IS_PLUGIN_PAGE_INVOICE(plugin_page));

    ENTER("(action %p, plugin_page %p)", action, plugin_page);
    priv = GNC_PLUGIN_PAGE_INVOICE_GET_PRIVATE(plugin_page);
    gnc_invoice_window_recordCB(NULL, priv->iw);
    LEAVE(" ");
}

static void
gnc_plugin_page_invoice_cmd_cancel (GtkAction *action,
                                    GncPluginPageInvoice *plugin_page)
{
    GncPluginPageInvoicePrivate *priv;

    g_return_if_fail(GNC_IS_PLUGIN_PAGE_INVOICE(plugin_page));

    ENTER("(action %p, plugin_page %p)", action, plugin_page);
    priv = GNC_PLUGIN_PAGE_INVOICE_GET_PRIVATE(plugin_page);
    gnc_invoice_window_cancelCB(NULL, priv->iw);
    LEAVE(" ");
}

static void
gnc_plugin_page_invoice_cmd_delete (GtkAction *action,
                                    GncPluginPageInvoice *plugin_page)
{
    GncPluginPageInvoicePrivate *priv;

    g_return_if_fail(GNC_IS_PLUGIN_PAGE_INVOICE(plugin_page));

    ENTER("(action %p, plugin_page %p)", action, plugin_page);
    priv = GNC_PLUGIN_PAGE_INVOICE_GET_PRIVATE(plugin_page);
    gnc_invoice_window_deleteCB(NULL, priv->iw);
    LEAVE(" ");
}

static void
gnc_plugin_page_invoice_cmd_blank (GtkAction *action,
                                   GncPluginPageInvoice *plugin_page)
{
    GncPluginPageInvoicePrivate *priv;

    g_return_if_fail(GNC_IS_PLUGIN_PAGE_INVOICE(plugin_page));

    ENTER("(action %p, plugin_page %p)", action, plugin_page);
    priv = GNC_PLUGIN_PAGE_INVOICE_GET_PRIVATE(plugin_page);
    gnc_invoice_window_blankCB(NULL, priv->iw);
    LEAVE(" ");
}

static void
gnc_plugin_page_invoice_cmd_duplicateEntry (GtkAction *action,
        GncPluginPageInvoice *plugin_page)
{
    GncPluginPageInvoicePrivate *priv;

    g_return_if_fail(GNC_IS_PLUGIN_PAGE_INVOICE(plugin_page));

    ENTER("(action %p, plugin_page %p)", action, plugin_page);
    priv = GNC_PLUGIN_PAGE_INVOICE_GET_PRIVATE(plugin_page);
    gnc_invoice_window_duplicateCB(NULL, priv->iw);
    LEAVE(" ");
}

static void
gnc_plugin_page_invoice_cmd_entryUp (GtkAction *action,
                                     GncPluginPageInvoice *plugin_page)
{
    GncPluginPageInvoicePrivate *priv;
    g_return_if_fail(GNC_IS_PLUGIN_PAGE_INVOICE(plugin_page));

    ENTER("(action %p, plugin_page %p)", action, plugin_page);
    priv = GNC_PLUGIN_PAGE_INVOICE_GET_PRIVATE(plugin_page);
    gnc_invoice_window_entryUpCB(NULL, priv->iw);
    LEAVE(" ");
}

static void
gnc_plugin_page_invoice_cmd_entryDown (GtkAction *action,
                                       GncPluginPageInvoice *plugin_page)
{
    GncPluginPageInvoicePrivate *priv;
    g_return_if_fail(GNC_IS_PLUGIN_PAGE_INVOICE(plugin_page));

    ENTER("(action %p, plugin_page %p)", action, plugin_page);
    priv = GNC_PLUGIN_PAGE_INVOICE_GET_PRIVATE(plugin_page);
    gnc_invoice_window_entryDownCB(NULL, priv->iw);
    LEAVE(" ");
}

static void
gnc_plugin_page_invoice_cmd_pay_invoice (GtkAction *action,
        GncPluginPageInvoice *plugin_page)
{
    GncPluginPageInvoicePrivate *priv;
    GtkWindow *parent;

    g_return_if_fail(GNC_IS_PLUGIN_PAGE_INVOICE(plugin_page));

    ENTER("(action %p, plugin_page %p)", action, plugin_page);
    priv = GNC_PLUGIN_PAGE_INVOICE_GET_PRIVATE(plugin_page);
    parent = GTK_WINDOW (gnc_plugin_page_get_window (GNC_PLUGIN_PAGE (plugin_page)));
    gnc_invoice_window_payment_cb (parent, priv->iw);
    LEAVE(" ");
}

static void
gnc_plugin_page_invoice_cmd_company_report (GtkAction *action,
        GncPluginPageInvoice *plugin_page)
{
    GncPluginPageInvoicePrivate *priv;
    GtkWindow *parent;

    g_return_if_fail(GNC_IS_PLUGIN_PAGE_INVOICE(plugin_page));

    ENTER("(action %p, plugin_page %p)", action, plugin_page);
    priv = GNC_PLUGIN_PAGE_INVOICE_GET_PRIVATE(plugin_page);
    parent = GTK_WINDOW (gnc_plugin_page_get_window (GNC_PLUGIN_PAGE (plugin_page)));
    gnc_invoice_window_report_owner_cb (parent, priv->iw);
    LEAVE(" ");
}

/************************************************************/
/*                    Auxiliary functions                   */
/************************************************************/

static void
gnc_plugin_page_redraw_help_cb (GnucashRegister *g_reg,
                                GncPluginPageInvoice *invoice_page)
{
    GncPluginPageInvoicePrivate *priv;
    GncWindow *window;
    const char *status;
    char *help;

    g_return_if_fail(GNC_IS_PLUGIN_PAGE_INVOICE(invoice_page));

    window = GNC_WINDOW(GNC_PLUGIN_PAGE(invoice_page)->window);

    /* Get the text from the ledger */
    priv = GNC_PLUGIN_PAGE_INVOICE_GET_PRIVATE(invoice_page);
    help = gnc_invoice_get_help(priv->iw);
    status = help ? help : g_strdup("");
    gnc_window_set_status(window, GNC_PLUGIN_PAGE(invoice_page), status);
    g_free(help);
}


void
gnc_plugin_page_invoice_update_title (GncPluginPage *plugin_page)
{
    GncPluginPageInvoice *page;
    GncPluginPageInvoicePrivate *priv;
    gchar *title;

    g_return_if_fail(GNC_IS_PLUGIN_PAGE_INVOICE(plugin_page));

    page = GNC_PLUGIN_PAGE_INVOICE(plugin_page);
    priv = GNC_PLUGIN_PAGE_INVOICE_GET_PRIVATE(page);
    title = gnc_invoice_get_title(priv->iw);
    main_window_update_page_name(plugin_page, title);
    g_free(title);
}

static void
gnc_plugin_page_invoice_refresh_cb (GHashTable *changes, gpointer user_data)
{
    GncPluginPageInvoice *page = user_data;
    GncPluginPageInvoicePrivate *priv;
    GtkWidget *reg;

    g_return_if_fail(GNC_IS_PLUGIN_PAGE_INVOICE(page));

    /* We're only looking for forced updates here. */
    if (changes)
        return;

    priv = GNC_PLUGIN_PAGE_INVOICE_GET_PRIVATE(page);
    reg = gnc_invoice_get_register(priv->iw);
    gnucash_register_refresh_from_prefs(GNUCASH_REGISTER(reg));
    gtk_widget_queue_draw(priv->widget);
}
