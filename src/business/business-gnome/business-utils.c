/*
 * business-utils.c -- General GUI Utilities for GNC Business Objects
 *
 * Written By: Derek Atkins <warlord@MIT.EDU>
 * Copyright (C) 2001, 2002 Derek Atkins
 */

#include "config.h"

#include <gnome.h>

#include "gncCustomer.h"
#include "gncJob.h"
#include "gncVendor.h"
#include "gncOwner.h"

#include "gnc-general-search.h"
#include "gncObject.h"
#include "business-utils.h"
#include "dialog-customer.h"
#include "dialog-job.h"
#include "dialog-vendor.h"

typedef enum {
  GNCSEARCH_TYPE_SELECT,
  GNCSEARCH_TYPE_EDIT
} GNCSearchType;

static GtkWidget * gnc_owner_new (GtkWidget *label, GtkWidget *hbox,
				  GNCBook *book, GncOwner *owner,
				  GNCSearchType type)
{
  GtkWidget *edit;
  GNCSearchCB search_cb = NULL;
  const char *type_name = NULL;
  const char *text = NULL;

  switch (type) {
  case GNCSEARCH_TYPE_SELECT:
    text = _("Select...");
    break;
  case GNCSEARCH_TYPE_EDIT:
    text = _("Edit...");
  };

  switch (owner->type) {
  case GNC_OWNER_NONE:
  case GNC_OWNER_UNDEFINED:
    return NULL;

  case GNC_OWNER_CUSTOMER:
    if (type == GNCSEARCH_TYPE_SELECT)
      search_cb = gnc_customer_search_select;
    else
      search_cb = gnc_customer_search_edit;
    type_name = GNC_CUSTOMER_MODULE_NAME;
    break;

  case GNC_OWNER_JOB:
    if (type == GNCSEARCH_TYPE_SELECT)
      search_cb = gnc_job_search_select;
    else
      search_cb = gnc_job_search_edit;
    type_name = GNC_JOB_MODULE_NAME;
    break;

  case GNC_OWNER_VENDOR:
    if (type == GNCSEARCH_TYPE_SELECT)
      search_cb = gnc_vendor_search_select;
    else
      search_cb = gnc_vendor_search_edit;
    type_name = GNC_VENDOR_MODULE_NAME;
    break;

  default:
    g_warning ("Unknown type");
    return NULL;
  }

  edit = gnc_general_search_new (type_name, text, search_cb, book);
  if (!edit)
    return NULL;

  gnc_general_search_set_selected (GNC_GENERAL_SEARCH (edit),
				   owner->owner.undefined);
  gtk_box_pack_start (GTK_BOX (hbox), edit, TRUE, TRUE, 0);
  if (label)
    gtk_label_set_text (GTK_LABEL (label), gncObjectGetTypeLabel (type_name));

  return edit;
}

GtkWidget * gnc_owner_select_create (GtkWidget *label, GtkWidget *hbox,
				     GNCBook *book, GncOwner *owner)
{
  g_return_val_if_fail (hbox != NULL, NULL);
  g_return_val_if_fail (book != NULL, NULL);
  g_return_val_if_fail (owner != NULL, NULL);

  return gnc_owner_new (label, hbox, book, owner, GNCSEARCH_TYPE_SELECT);
}

GtkWidget * gnc_owner_edit_create (GtkWidget *label, GtkWidget *hbox,
				   GNCBook *book, GncOwner *owner)
{
  g_return_val_if_fail (hbox != NULL, NULL);
  g_return_val_if_fail (book != NULL, NULL);
  g_return_val_if_fail (owner != NULL, NULL);

  return gnc_owner_new (label, hbox, book, owner, GNCSEARCH_TYPE_EDIT);
}

void gnc_owner_get_owner (GtkWidget *widget, GncOwner *owner)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (owner != NULL);

  /* We'll assume that the owner has the proper 'type' because we
   * can't change it here.  Hopefully the caller has it set properly
   */
  owner->owner.undefined =
    gnc_general_search_get_selected (GNC_GENERAL_SEARCH (widget));
}

void gnc_owner_set_owner (GtkWidget *widget, GncOwner *owner)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (owner != NULL);

  /* We'll assume that the owner has the proper 'type' because we
   * can't change it here.  Hopefully the caller has it set properly
   */

  gnc_general_search_set_selected (GNC_GENERAL_SEARCH (widget),
				   owner->owner.undefined);
}
