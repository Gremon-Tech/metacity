/*
 * Copyright (C) 2001 Havoc Pennington
 * Copyright (C) 2016 Alberts Muktupāvels
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <glib/gi18n-lib.h>

#include "meta-color-spec.h"
#include "meta-gradient-private.h"
#include "meta-gradient-spec.h"
#include "meta-theme.h"

struct _MetaGradientSpec
{
  MetaGradientType  type;
  GSList           *color_specs;
};

struct _MetaAlphaGradientSpec
{
  MetaGradientType  type;
  guchar           *alphas;
  gint              n_alphas;
};

static void
free_color_spec (gpointer spec,
                 gpointer user_data)
{
  meta_color_spec_free (spec);
}

MetaGradientSpec *
meta_gradient_spec_new (MetaGradientType type)
{
  MetaGradientSpec *spec;

  spec = g_new (MetaGradientSpec, 1);

  spec->type = type;
  spec->color_specs = NULL;

  return spec;
}

void
meta_gradient_spec_free (MetaGradientSpec *spec)
{
  g_return_if_fail (spec != NULL);

  g_slist_foreach (spec->color_specs, free_color_spec, NULL);
  g_slist_free (spec->color_specs);

  g_free (spec);
}

void
meta_gradient_spec_add_color_spec (MetaGradientSpec *spec,
                                   MetaColorSpec    *color_spec)
{
  spec->color_specs = g_slist_append (spec->color_specs, color_spec);
}

GdkPixbuf *
meta_gradient_spec_render (const MetaGradientSpec *spec,
                           GtkStyleContext        *context,
                           gint                    width,
                           gint                    height)
{
  gint n_colors;
  GdkRGBA *colors;
  GSList *tmp;
  gint i;
  GdkPixbuf *pixbuf;

  n_colors = g_slist_length (spec->color_specs);

  if (n_colors == 0)
    return NULL;

  colors = g_new (GdkRGBA, n_colors);

  i = 0;
  tmp = spec->color_specs;
  while (tmp != NULL)
    {
      meta_color_spec_render (tmp->data, context, &colors[i]);

      tmp = tmp->next;
      ++i;
    }

  pixbuf = meta_gradient_create_multi (width, height, colors,
                                       n_colors, spec->type);

  g_free (colors);

  return pixbuf;
}

gboolean
meta_gradient_spec_validate (MetaGradientSpec  *spec,
                             GError           **error)
{
  g_return_val_if_fail (spec != NULL, FALSE);

  if (g_slist_length (spec->color_specs) < 2)
    {
      g_set_error (error, META_THEME_ERROR, META_THEME_ERROR_FAILED,
                   _("Gradients should have at least two colors"));

      return FALSE;
    }

  return TRUE;
}

MetaAlphaGradientSpec *
meta_alpha_gradient_spec_new (MetaGradientType type,
                              gint             n_alphas)
{
  MetaAlphaGradientSpec *spec;

  g_return_val_if_fail (n_alphas > 0, NULL);

  spec = g_new0 (MetaAlphaGradientSpec, 1);

  spec->type = type;
  spec->alphas = g_new0 (guchar, n_alphas);
  spec->n_alphas = n_alphas;

  return spec;
}

void
meta_alpha_gradient_spec_free (MetaAlphaGradientSpec *spec)
{
  g_return_if_fail (spec != NULL);

  g_free (spec->alphas);
  g_free (spec);
}

void
meta_alpha_gradient_spec_add_alpha (MetaAlphaGradientSpec *spec,
                                    gint                   n_alpha,
                                    gdouble                alpha)
{
  spec->alphas[n_alpha] = (guchar) (alpha * 255);
}

guchar
meta_alpha_gradient_spec_get_alpha (MetaAlphaGradientSpec *spec,
                                    gint                   n_alpha)
{
  return spec->alphas[n_alpha];
}

gboolean
meta_alpha_gradient_spec_needs_alpha (MetaAlphaGradientSpec *spec)
{
  return spec && (spec->n_alphas > 1 || spec->alphas[0] != 0xff);
}

GdkPixbuf *
meta_alpha_gradient_spec_apply_alpha (MetaAlphaGradientSpec *spec,
                                      GdkPixbuf             *pixbuf,
                                      gboolean               force_copy)
{
  GdkPixbuf *new_pixbuf;
  gboolean needs_alpha;

  g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);

  needs_alpha = meta_alpha_gradient_spec_needs_alpha (spec);

  if (!needs_alpha)
    return pixbuf;

  if (!gdk_pixbuf_get_has_alpha (pixbuf))
    {
      new_pixbuf = gdk_pixbuf_add_alpha (pixbuf, FALSE, 0, 0, 0);
      g_object_unref (G_OBJECT (pixbuf));
      pixbuf = new_pixbuf;
    }
  else if (force_copy)
    {
      new_pixbuf = gdk_pixbuf_copy (pixbuf);
      g_object_unref (G_OBJECT (pixbuf));
      pixbuf = new_pixbuf;
    }

  g_assert (gdk_pixbuf_get_has_alpha (pixbuf));

  meta_gradient_add_alpha (pixbuf, spec->alphas, spec->n_alphas, spec->type);

  return pixbuf;
}

GdkPixbuf *
meta_alpha_gradient_spec_render (MetaAlphaGradientSpec *spec,
                                 gint                   width,
                                 gint                   height,
                                 GdkRGBA                color)
{
  gboolean has_alpha;
  GdkPixbuf *pixbuf;
  guint32 rgba;

  has_alpha = spec && (spec->n_alphas > 1 || spec->alphas[0] != 0xff);
  pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, has_alpha, 8, width, height);

  rgba = 0xff;
  rgba |= (gint) (color.red * 255) << 24;
  rgba |= (gint) (color.green * 255) << 16;
  rgba |= (gint) (color.blue * 255) << 8;

  if (!has_alpha)
    {
      gdk_pixbuf_fill (pixbuf, rgba);
    }
  else if (spec->n_alphas == 1)
    {
      rgba &= ~0xff;
      rgba |= spec->alphas[0];

      gdk_pixbuf_fill (pixbuf, rgba);
    }
  else
    {
      gdk_pixbuf_fill (pixbuf, rgba);

      meta_gradient_add_alpha (pixbuf, spec->alphas, spec->n_alphas,
                               spec->type);
    }

  return pixbuf;
}