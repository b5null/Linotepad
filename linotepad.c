#include <gtk/gtk.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
    GtkWidget *window;
    GtkWidget *textview;
    GtkTextBuffer *buffer;
    GtkWidget *statusbar_pos;   /* Ln/Col label */
    guint      status_ctx;
    GtkWidget *menu_wordwrap;   /* checkmenuitem */
    GtkWidget *menu_statusbar;  /* checkmenuitem */
    gchar     *filename;        /* NULL = Untitled */
    gboolean   dirty;
    gboolean   show_status;

    /* Find/Replace state */
    gchar     *find_text;
    gchar     *replace_text;
    gboolean   match_case;

    /* Zoom state */
    gint       font_size;
} AppState;

static gint open_windows = 0;
static GtkWidget *create_window(const gchar *path);

static void update_title(AppState *app) {
    gchar *base = app->filename
        ? g_filename_display_basename(app->filename)
        : g_strdup("Untitled");
    gchar *title = g_strdup_printf("%s%s - Linotepad",
                                    app->dirty ? "*" : "", base);
    gtk_window_set_title(GTK_WINDOW(app->window), title);
    g_free(title);
    g_free(base);
}

static void mark_dirty(AppState *app) {
    if (!app->dirty) {
        app->dirty = TRUE;
        update_title(app);
    }
}

static void update_status(AppState *app) {
    if (!app->show_status) return;
    GtkTextIter iter;
    GtkTextMark *mark = gtk_text_buffer_get_insert(app->buffer);
    gtk_text_buffer_get_iter_at_mark(app->buffer, &iter, mark);
    int line = gtk_text_iter_get_line(&iter) + 1;
    int col  = gtk_text_iter_get_line_offset(&iter) + 1;
    gchar *msg = g_strdup_printf("  Ln %d, Col %d", line, col);
    gtk_statusbar_pop(GTK_STATUSBAR(app->statusbar_pos), app->status_ctx);
    gtk_statusbar_push(GTK_STATUSBAR(app->statusbar_pos), app->status_ctx, msg);
    g_free(msg);
}

/* ---------------- Zoom ---------------- */

static void apply_font_size(AppState *app) {
    gchar *desc_str = g_strdup_printf("Courier %d", app->font_size);
    PangoFontDescription *desc = pango_font_description_from_string(desc_str);
    gtk_widget_override_font(app->textview, desc);
    pango_font_description_free(desc);
    g_free(desc_str);
}

static void zoom_in(AppState *app) {
    if (app->font_size < 72) app->font_size += 1;
    apply_font_size(app);
}

static void zoom_out(AppState *app) {
    if (app->font_size > 4) app->font_size -= 1;
    apply_font_size(app);
}

static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, AppState *app) {
    (void)widget;
    if (event->state & GDK_CONTROL_MASK) {
        if (event->keyval == GDK_KEY_plus || event->keyval == GDK_KEY_equal ||
            event->keyval == GDK_KEY_KP_Add) {
            zoom_in(app);
            return TRUE;
        }
        if (event->keyval == GDK_KEY_minus || event->keyval == GDK_KEY_KP_Subtract) {
            zoom_out(app);
            return TRUE;
        }
    }
    return FALSE;
}

static gboolean on_scroll(GtkWidget *widget, GdkEventScroll *event, AppState *app) {
    (void)widget;
    if (event->state & GDK_CONTROL_MASK) {
        if (event->direction == GDK_SCROLL_UP) {
            zoom_in(app);
            return TRUE;
        }
        if (event->direction == GDK_SCROLL_DOWN) {
            zoom_out(app);
            return TRUE;
        }
    }
    return FALSE;
}

/* ---------------- File operations ---------------- */

static void buffer_changed_cb(GtkTextBuffer *buf, AppState *app) {
    (void)buf;
    mark_dirty(app);
}

static void cursor_moved_cb(GtkTextBuffer *buf, GParamSpec *pspec, AppState *app) {
    (void)buf; (void)pspec;
    update_status(app);
}

static void set_buffer_text_from_file(AppState *app, const gchar *path) {
    gchar *contents = NULL;
    gsize len = 0;
    GError *err = NULL;
    if (!g_file_get_contents(path, &contents, &len, &err)) {
        if (g_error_matches(err, G_FILE_ERROR, G_FILE_ERROR_NOENT)) {
            g_free(app->filename);
            app->filename = g_strdup(path);
            app->dirty = FALSE;
            update_title(app);
            update_status(app);
            g_error_free(err);
            return;
        }

        GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(app->window),
            GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
            "Could not open file:\n%s", err->message);
        gtk_dialog_run(GTK_DIALOG(d));
        gtk_widget_destroy(d);
        g_error_free(err);
        return;
    }
    g_signal_handlers_block_by_func(app->buffer, buffer_changed_cb, app);
    gtk_text_buffer_set_text(app->buffer, contents, (gint)len);
    g_signal_handlers_unblock_by_func(app->buffer, buffer_changed_cb, app);
    g_free(contents);

    g_free(app->filename);
    app->filename = g_strdup(path);
    app->dirty = FALSE;
    update_title(app);
    update_status(app);
}

static gboolean save_to_file(AppState *app, const gchar *path) {
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(app->buffer, &start, &end);
    gchar *text = gtk_text_buffer_get_text(app->buffer, &start, &end, FALSE);
    gchar *saved_path = g_strdup(path);
    GError *err = NULL;
    gboolean ok = g_file_set_contents(path, text, -1, &err);
    g_free(text);
    if (!ok) {
        g_free(saved_path);
        GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(app->window),
            GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
            "Could not save file:\n%s", err->message);
        gtk_dialog_run(GTK_DIALOG(d));
        gtk_widget_destroy(d);
        g_error_free(err);
        return FALSE;
    }
    g_free(app->filename);
    app->filename = saved_path;
    app->dirty = FALSE;
    update_title(app);
    return TRUE;
}

static gboolean pick_save_path(AppState *app) {
    GtkWidget *dlg = gtk_file_chooser_dialog_new("Save As", GTK_WINDOW(app->window),
        GTK_FILE_CHOOSER_ACTION_SAVE,
        "_Cancel", GTK_RESPONSE_CANCEL, "_Save", GTK_RESPONSE_ACCEPT, NULL);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dlg), TRUE);
    if (app->filename)
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dlg), app->filename);
    else
        gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dlg), "Untitled.txt");

    gboolean result = FALSE;
    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
        gchar *path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
        result = save_to_file(app, path);
        g_free(path);
    }
    gtk_widget_destroy(dlg);
    return result;
}

static gboolean maybe_save_changes(AppState *app) {
    if (!app->dirty) return TRUE;

    GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(app->window),
        GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
        "Save changes?");
    gtk_dialog_add_buttons(GTK_DIALOG(d),
        "_Yes", GTK_RESPONSE_YES, "_No", GTK_RESPONSE_NO,
        "_Cancel", GTK_RESPONSE_CANCEL, NULL);
    gint resp = gtk_dialog_run(GTK_DIALOG(d));
    gtk_widget_destroy(d);

    if (resp == GTK_RESPONSE_CANCEL) return FALSE;
    if (resp == GTK_RESPONSE_NO) return TRUE;

    if (app->filename)
        return save_to_file(app, app->filename);
    return pick_save_path(app);
}

static void on_new_window(GtkMenuItem *item, AppState *app) {
    (void)item;
    (void)app;
    create_window(NULL);
}

static void on_open(GtkMenuItem *item, AppState *app) {
    (void)item;
    if (!maybe_save_changes(app)) return;
    GtkWidget *dlg = gtk_file_chooser_dialog_new("Open", GTK_WINDOW(app->window),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel", GTK_RESPONSE_CANCEL, "_Open", GTK_RESPONSE_ACCEPT, NULL);
    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
        gchar *path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
        set_buffer_text_from_file(app, path);
        g_free(path);
    }
    gtk_widget_destroy(dlg);
}

static void on_save(GtkMenuItem *item, AppState *app) {
    (void)item;
    if (app->filename) save_to_file(app, app->filename);
    else pick_save_path(app);
}

static void on_save_as(GtkMenuItem *item, AppState *app) {
    (void)item;
    pick_save_path(app);
}

typedef struct {
    gchar *text;
    PangoLayout *layout;
    int lines_per_page;
    int n_lines;
} PrintJob;

static void print_begin(GtkPrintOperation *op, GtkPrintContext *ctx, gpointer data) {
    PrintJob *job = data;
    job->layout = gtk_print_context_create_pango_layout(ctx);
    pango_layout_set_text(job->layout, job->text, -1);
    pango_layout_set_width(job->layout,
        (int)(gtk_print_context_get_width(ctx) * PANGO_SCALE));

    int line_height;
    pango_layout_get_pixel_size(job->layout, NULL, &line_height);
    PangoLayoutIter *it = pango_layout_get_iter(job->layout);
    job->n_lines = 0;
    do { job->n_lines++; } while (pango_layout_iter_next_line(it));
    pango_layout_iter_free(it);

    int page_h = (int)gtk_print_context_get_height(ctx);
    int single_line_h = line_height > 0 ? line_height / (job->n_lines ? job->n_lines : 1) : 14;
    if (single_line_h <= 0) single_line_h = 14;
    job->lines_per_page = page_h / single_line_h;
    if (job->lines_per_page <= 0) job->lines_per_page = 1;

    int n_pages = (job->n_lines + job->lines_per_page - 1) / job->lines_per_page;
    if (n_pages <= 0) n_pages = 1;
    gtk_print_operation_set_n_pages(op, n_pages);
}

static void print_draw_page(GtkPrintOperation *op, GtkPrintContext *ctx,
                             gint page_nr, gpointer data) {
    (void)op;
    PrintJob *job = data;
    cairo_t *cr = gtk_print_context_get_cairo_context(ctx);
    cairo_move_to(cr, 0, 0);

    PangoLayoutIter *it = pango_layout_get_iter(job->layout);
    int start_line = page_nr * job->lines_per_page;
    int i = 0;
    int y_offset = 0;
    gboolean first_on_page = TRUE;
    int base_y = 0;
    do {
        if (i >= start_line && i < start_line + job->lines_per_page) {
            PangoRectangle rect;
            pango_layout_iter_get_line_extents(it, NULL, &rect);
            if (first_on_page) { base_y = rect.y; first_on_page = FALSE; }
            y_offset = rect.y - base_y;
            PangoLayoutLine *pline = pango_layout_iter_get_line_readonly(it);
            cairo_move_to(cr, 0, (double)y_offset / PANGO_SCALE);
            pango_cairo_show_layout_line(cr, pline);
        }
        i++;
    } while (i < start_line + job->lines_per_page && pango_layout_iter_next_line(it));
    pango_layout_iter_free(it);
}

static void print_end(GtkPrintOperation *op, GtkPrintContext *ctx, gpointer data) {
    (void)op; (void)ctx;
    PrintJob *job = data;
    g_free(job->text);
    if (job->layout) g_object_unref(job->layout);
    g_free(job);
}

static void on_print(GtkMenuItem *item, AppState *app) {
    (void)item;
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(app->buffer, &start, &end);

    PrintJob *job = g_new0(PrintJob, 1);
    job->text = gtk_text_buffer_get_text(app->buffer, &start, &end, FALSE);

    GtkPrintOperation *op = gtk_print_operation_new();
    g_signal_connect(op, "begin-print", G_CALLBACK(print_begin), job);
    g_signal_connect(op, "draw-page", G_CALLBACK(print_draw_page), job);
    g_signal_connect(op, "end-print", G_CALLBACK(print_end), job);

    GError *err = NULL;
    gtk_print_operation_run(op, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
                             GTK_WINDOW(app->window), &err);
    if (err) {
        GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(app->window),
            GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
            "Print failed:\n%s", err->message);
        gtk_dialog_run(GTK_DIALOG(d));
        gtk_widget_destroy(d);
        g_error_free(err);
    }
    g_object_unref(op);
}

static void on_file_exit(GtkMenuItem *item, AppState *app) {
    (void)item;
    if (!maybe_save_changes(app)) return;
    gtk_widget_destroy(app->window);
}

/* ---------------- Edit operations ---------------- */

static void on_cut(GtkMenuItem *item, AppState *app) {
    (void)item;
    GtkClipboard *cb = gtk_widget_get_clipboard(app->textview, GDK_SELECTION_CLIPBOARD);
    gtk_text_buffer_cut_clipboard(app->buffer, cb, TRUE);
}
static void on_copy(GtkMenuItem *item, AppState *app) {
    (void)item;
    GtkClipboard *cb = gtk_widget_get_clipboard(app->textview, GDK_SELECTION_CLIPBOARD);
    gtk_text_buffer_copy_clipboard(app->buffer, cb);
}
static void on_paste(GtkMenuItem *item, AppState *app) {
    (void)item;
    GtkClipboard *cb = gtk_widget_get_clipboard(app->textview, GDK_SELECTION_CLIPBOARD);
    gtk_text_buffer_paste_clipboard(app->buffer, cb, NULL, TRUE);
}
static void on_delete(GtkMenuItem *item, AppState *app) {
    (void)item;
    GtkTextIter start, end;
    gint pos, chars;

    if (gtk_text_buffer_get_selection_bounds(app->buffer, &start, &end)) {
        gtk_text_buffer_delete(app->buffer, &start, &end);
        return;
    }

    gtk_text_buffer_get_iter_at_mark(app->buffer, &start,
        gtk_text_buffer_get_insert(app->buffer));
    pos = gtk_text_iter_get_offset(&start);
    chars = gtk_text_buffer_get_char_count(app->buffer);

    if (chars <= 0)
        return;

    if (pos >= chars)
        pos = chars - 1;

    gtk_text_buffer_get_iter_at_offset(app->buffer, &start, pos);
    gtk_text_buffer_get_iter_at_offset(app->buffer, &end, pos + 1);
    gtk_text_buffer_delete(app->buffer, &start, &end);
}
static void on_select_all(GtkMenuItem *item, AppState *app) {
    (void)item;
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(app->buffer, &start, &end);
    gtk_text_buffer_select_range(app->buffer, &start, &end);
}
static void on_insert_time(GtkMenuItem *item, AppState *app) {
    (void)item;
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    char buf[64];
    strftime(buf, sizeof(buf), "%m/%d/%Y %I:%M %p", lt);
    gtk_text_buffer_insert_at_cursor(app->buffer, buf, -1);
}

/* ---------------- Find / Replace ---------------- */

static gboolean find_next(AppState *app, gboolean wrap) {
    if (!app->find_text || !*app->find_text) return FALSE;

    GtkTextIter cursor_iter, match_start, match_end;
    gtk_text_buffer_get_iter_at_mark(app->buffer, &cursor_iter,
        gtk_text_buffer_get_insert(app->buffer));

    GtkTextSearchFlags flags = GTK_TEXT_SEARCH_TEXT_ONLY;
    if (!app->match_case) flags |= GTK_TEXT_SEARCH_CASE_INSENSITIVE;

    gboolean found = gtk_text_iter_forward_search(&cursor_iter, app->find_text, flags,
                                                   &match_start, &match_end, NULL);
    if (!found && wrap) {
        GtkTextIter buf_start;
        gtk_text_buffer_get_start_iter(app->buffer, &buf_start);
        found = gtk_text_iter_forward_search(&buf_start, app->find_text, flags,
                                              &match_start, &match_end, &cursor_iter);
    }
    if (found) {
        /* insert mark goes to match_end (not match_start): otherwise the
         * next Find Next call starts searching from the same spot and
         * re-matches the same occurrence forever. */
        gtk_text_buffer_select_range(app->buffer, &match_end, &match_start);
        gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(app->textview), &match_end,
                                      0.1, FALSE, 0, 0);
    }
    return found;
}

static void report_not_found(GtkWidget *parent_dlg) {
    GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(parent_dlg),
        GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
        "Text not found.");
    gtk_dialog_run(GTK_DIALOG(d));
    gtk_widget_destroy(d);
}

static void sync_state_from_dialog(GtkWidget *dlg) {
    AppState *app = g_object_get_data(G_OBJECT(dlg), "app");
    GtkWidget *find_entry = g_object_get_data(G_OBJECT(dlg), "find_entry");
    GtkWidget *repl_entry = g_object_get_data(G_OBJECT(dlg), "repl_entry");
    GtkWidget *case_chk = g_object_get_data(G_OBJECT(dlg), "case_chk");

    g_free(app->find_text);
    app->find_text = g_strdup(gtk_entry_get_text(GTK_ENTRY(find_entry)));

    if (repl_entry) {
        g_free(app->replace_text);
        app->replace_text = g_strdup(gtk_entry_get_text(GTK_ENTRY(repl_entry)));
    }
    app->match_case = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(case_chk));
}

static void on_dialog_find_next(GtkButton *btn, gpointer user_data) {
    (void)btn;
    GtkWidget *dlg = GTK_WIDGET(user_data);
    AppState *app = g_object_get_data(G_OBJECT(dlg), "app");
    sync_state_from_dialog(dlg);
    if (!find_next(app, TRUE)) report_not_found(dlg);
}

static void on_dialog_replace(GtkButton *btn, gpointer user_data) {
    (void)btn;
    GtkWidget *dlg = GTK_WIDGET(user_data);
    AppState *app = g_object_get_data(G_OBJECT(dlg), "app");
    sync_state_from_dialog(dlg);

    GtkTextIter sel_start, sel_end;
    if (gtk_text_buffer_get_selection_bounds(app->buffer, &sel_start, &sel_end)) {
        gchar *selected = gtk_text_buffer_get_text(app->buffer, &sel_start, &sel_end, FALSE);
        gboolean matches = app->match_case
            ? (strcmp(selected, app->find_text) == 0)
            : (g_ascii_strcasecmp(selected, app->find_text) == 0);
        g_free(selected);
        if (matches) {
            gtk_text_buffer_delete(app->buffer, &sel_start, &sel_end);
            gtk_text_buffer_insert(app->buffer, &sel_start,
                                    app->replace_text ? app->replace_text : "", -1);
        }
    }
    if (!find_next(app, TRUE)) report_not_found(dlg);
}

static void on_dialog_replace_all(GtkButton *btn, gpointer user_data) {
    (void)btn;
    GtkWidget *dlg = GTK_WIDGET(user_data);
    AppState *app = g_object_get_data(G_OBJECT(dlg), "app");
    sync_state_from_dialog(dlg);

    if (!app->find_text || !*app->find_text) {
        report_not_found(dlg);
        return;
    }

    GtkTextIter iter;
    gtk_text_buffer_get_start_iter(app->buffer, &iter);
    gtk_text_buffer_place_cursor(app->buffer, &iter);

    GtkTextSearchFlags flags = GTK_TEXT_SEARCH_TEXT_ONLY;
    if (!app->match_case) flags |= GTK_TEXT_SEARCH_CASE_INSENSITIVE;

    int count = 0;
    GtkTextIter search_from, match_start, match_end;
    gtk_text_buffer_get_start_iter(app->buffer, &search_from);

    while (gtk_text_iter_forward_search(&search_from, app->find_text, flags,
                                         &match_start, &match_end, NULL)) {
        gtk_text_buffer_delete(app->buffer, &match_start, &match_end);
        gtk_text_buffer_insert(app->buffer, &match_start,
                                app->replace_text ? app->replace_text : "", -1);
        count++;
        search_from = match_start;
    }

    gchar *msg = g_strdup_printf("Replaced %d occurrence%s.", count, count == 1 ? "" : "s");
    GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(dlg),
        GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "%s", msg);
    g_free(msg);
    gtk_dialog_run(GTK_DIALOG(d));
    gtk_widget_destroy(d);
}

static void show_find_dialog(AppState *app, gboolean with_replace) {
    GtkWidget *dlg = gtk_dialog_new_with_buttons(
        with_replace ? "Replace" : "Find",
        GTK_WINDOW(app->window), GTK_DIALOG_DESTROY_WITH_PARENT,
        "_Close", GTK_RESPONSE_CLOSE, NULL);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 6);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);
    gtk_container_add(GTK_CONTAINER(content), grid);

    GtkWidget *find_label = gtk_label_new("Find what:");
    GtkWidget *find_entry = gtk_entry_new();
    if (app->find_text) gtk_entry_set_text(GTK_ENTRY(find_entry), app->find_text);
    gtk_grid_attach(GTK_GRID(grid), find_label, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), find_entry, 1, 0, 2, 1);

    GtkWidget *repl_entry = NULL;
    if (with_replace) {
        GtkWidget *repl_label = gtk_label_new("Replace with:");
        repl_entry = gtk_entry_new();
        if (app->replace_text) gtk_entry_set_text(GTK_ENTRY(repl_entry), app->replace_text);
        gtk_grid_attach(GTK_GRID(grid), repl_label, 0, 1, 1, 1);
        gtk_grid_attach(GTK_GRID(grid), repl_entry, 1, 1, 2, 1);
    }

    GtkWidget *case_chk = gtk_check_button_new_with_label("Match case");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(case_chk), app->match_case);
    gtk_grid_attach(GTK_GRID(grid), case_chk, 0, 2, 1, 1);

    GtkWidget *find_btn = gtk_button_new_with_label("Find Next");
    gtk_grid_attach(GTK_GRID(grid), find_btn, 1, 2, 1, 1);

    GtkWidget *repl_btn = NULL, *repl_all_btn = NULL;
    if (with_replace) {
        repl_btn = gtk_button_new_with_label("Replace");
        repl_all_btn = gtk_button_new_with_label("Replace All");
        gtk_grid_attach(GTK_GRID(grid), repl_btn, 2, 2, 1, 1);
        gtk_grid_attach(GTK_GRID(grid), repl_all_btn, 1, 3, 2, 1);
    }

    g_object_set_data(G_OBJECT(dlg), "app", app);
    g_object_set_data(G_OBJECT(dlg), "find_entry", find_entry);
    g_object_set_data(G_OBJECT(dlg), "repl_entry", repl_entry);
    g_object_set_data(G_OBJECT(dlg), "case_chk", case_chk);

    g_signal_connect(find_btn, "clicked", G_CALLBACK(on_dialog_find_next), dlg);
    if (with_replace) {
        g_signal_connect(repl_btn, "clicked", G_CALLBACK(on_dialog_replace), dlg);
        g_signal_connect(repl_all_btn, "clicked", G_CALLBACK(on_dialog_replace_all), dlg);
    }

    g_signal_connect(find_entry, "activate", G_CALLBACK(on_dialog_find_next), dlg);

    gtk_widget_show_all(dlg);
    gtk_widget_grab_focus(find_entry);

    gtk_dialog_run(GTK_DIALOG(dlg));

    sync_state_from_dialog(dlg);
    gtk_widget_destroy(dlg);
}

static void on_find(GtkMenuItem *item, AppState *app) {
    (void)item;
    show_find_dialog(app, FALSE);
}
static void on_find_next(GtkMenuItem *item, AppState *app) {
    (void)item;
    if (!find_next(app, TRUE)) report_not_found(app->window);
}
static void on_replace(GtkMenuItem *item, AppState *app) {
    (void)item;
    show_find_dialog(app, TRUE);
}

static void on_goto(GtkMenuItem *item, AppState *app) {
    (void)item;
    GtkWidget *dlg = gtk_dialog_new_with_buttons("Go To Line",
        GTK_WINDOW(app->window), GTK_DIALOG_MODAL,
        "_Cancel", GTK_RESPONSE_CANCEL, "_OK", GTK_RESPONSE_OK, NULL);
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_input_purpose(GTK_ENTRY(entry), GTK_INPUT_PURPOSE_DIGITS);
    gtk_container_set_border_width(GTK_CONTAINER(content), 10);
    gtk_container_add(GTK_CONTAINER(content), entry);
    gtk_widget_show_all(dlg);

    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK) {
        int line = atoi(gtk_entry_get_text(GTK_ENTRY(entry)));
        if (line > 0) {
            GtkTextIter iter;
            gtk_text_buffer_get_iter_at_line(app->buffer, &iter, line - 1);
            gtk_text_buffer_place_cursor(app->buffer, &iter);
            gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(app->textview), &iter,
                                          0.1, FALSE, 0, 0);
        }
    }
    gtk_widget_destroy(dlg);
}

/* ---------------- Format ---------------- */

static void on_word_wrap(GtkCheckMenuItem *item, AppState *app) {
    gboolean on = gtk_check_menu_item_get_active(item);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(app->textview),
        on ? GTK_WRAP_WORD : GTK_WRAP_NONE);
}

static void on_font(GtkMenuItem *item, AppState *app) {
    (void)item;
    GtkWidget *dlg = gtk_font_chooser_dialog_new("Choose Font", GTK_WINDOW(app->window));
    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK) {
        gchar *fontname = gtk_font_chooser_get_font(GTK_FONT_CHOOSER(dlg));
        PangoFontDescription *desc = pango_font_description_from_string(fontname);
        gtk_widget_override_font(app->textview, desc);
        gint size = pango_font_description_get_size(desc);
        if (size > 0) app->font_size = size / PANGO_SCALE;
        pango_font_description_free(desc);
        g_free(fontname);
    }
    gtk_widget_destroy(dlg);
}

/* ---------------- View ---------------- */

static void on_view_status(GtkCheckMenuItem *item, AppState *app) {
    app->show_status = gtk_check_menu_item_get_active(item);
    gtk_widget_set_visible(app->statusbar_pos, app->show_status);
    if (app->show_status) update_status(app);
}

/* ---------------- Help ---------------- */

static void on_about(GtkMenuItem *item, AppState *app) {
    (void)item;
    GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(app->window),
        GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
        "Linotepad\n\nLinux (GTK3) port of Dave Plummer's Tiny Editor.");
    gtk_dialog_run(GTK_DIALOG(d));
    gtk_widget_destroy(d);
}

static void on_help_view(GtkMenuItem *item, AppState *app) {
    (void)item;
    GAppInfo *info = g_app_info_get_default_for_uri_scheme("https");
    GList *uris = NULL;
    uris = g_list_append(uris, "https://github.com/davepl");
    if (info) g_app_info_launch_uris(info, uris, NULL, NULL);
    g_list_free(uris);
}

/* ---------------- Menu building ---------------- */

static GtkWidget *add_item(GtkWidget *menu, const gchar *label,
                            GCallback cb, AppState *app, const gchar *accel,
                            GtkAccelGroup *ag) {
    GtkWidget *mi = gtk_menu_item_new_with_mnemonic(label);
    if (cb) g_signal_connect(mi, "activate", cb, app);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    if (accel && ag) {
        guint key; GdkModifierType mods;
        gtk_accelerator_parse(accel, &key, &mods);
        gtk_widget_add_accelerator(mi, "activate", ag, key, mods, GTK_ACCEL_VISIBLE);
    }
    return mi;
}

static void add_separator(GtkWidget *menu) {
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
}

static gboolean on_delete_event(GtkWidget *w, GdkEvent *e, AppState *app) {
    (void)w; (void)e;
    return !maybe_save_changes(app);
}

static void on_window_destroy(GtkWidget *w, AppState *app) {
    (void)w;
    g_free(app->filename);
    g_free(app->find_text);
    g_free(app->replace_text);
    g_free(app);

    open_windows--;
    if (open_windows <= 0)
        gtk_main_quit();
}

static GtkWidget *create_window(const gchar *path) {
    AppState *app = g_new0(AppState, 1);
    app->show_status = TRUE;
    app->match_case = FALSE;
    app->font_size = 10;

    app->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    open_windows++;
    gtk_window_set_default_size(GTK_WINDOW(app->window), 800, 640);
    g_signal_connect(app->window, "delete-event", G_CALLBACK(on_delete_event), app);
    g_signal_connect(app->window, "destroy", G_CALLBACK(on_window_destroy), app);
    g_signal_connect(app->window, "key-press-event", G_CALLBACK(on_key_press), app);

    GtkAccelGroup *accel = gtk_accel_group_new();
    gtk_window_add_accel_group(GTK_WINDOW(app->window), accel);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(app->window), vbox);

    GtkWidget *menubar = gtk_menu_bar_new();
    gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, FALSE, 0);

    GtkWidget *file_item = gtk_menu_item_new_with_mnemonic("_File");
    GtkWidget *file_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(file_item), file_menu);
    add_item(file_menu, "_New Window", G_CALLBACK(on_new_window), app, "<Control>N", accel);
    add_item(file_menu, "_Open...", G_CALLBACK(on_open), app, "<Control>O", accel);
    add_item(file_menu, "_Save", G_CALLBACK(on_save), app, "<Control>S", accel);
    add_item(file_menu, "Save _As...", G_CALLBACK(on_save_as), app, "<Control><Shift>S", accel);
    add_separator(file_menu);
    add_item(file_menu, "_Print...", G_CALLBACK(on_print), app, "<Control>P", accel);
    add_separator(file_menu);
    add_item(file_menu, "E_xit", G_CALLBACK(on_file_exit), app, NULL, NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), file_item);

    GtkWidget *edit_item = gtk_menu_item_new_with_mnemonic("_Edit");
    GtkWidget *edit_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(edit_item), edit_menu);
    add_item(edit_menu, "Cu_t", G_CALLBACK(on_cut), app, "<Control>X", accel);
    add_item(edit_menu, "_Copy", G_CALLBACK(on_copy), app, "<Control>C", accel);
    add_item(edit_menu, "_Paste", G_CALLBACK(on_paste), app, "<Control>V", accel);
    add_item(edit_menu, "De_lete", G_CALLBACK(on_delete), app, "Delete", accel);
    add_separator(edit_menu);
    add_item(edit_menu, "_Find...", G_CALLBACK(on_find), app, "<Control>F", accel);
    add_item(edit_menu, "Find _Next", G_CALLBACK(on_find_next), app, "F3", accel);
    add_item(edit_menu, "_Replace...", G_CALLBACK(on_replace), app, "<Control>H", accel);
    add_item(edit_menu, "_Go To...", G_CALLBACK(on_goto), app, "<Control>G", accel);
    add_separator(edit_menu);
    add_item(edit_menu, "Select _All", G_CALLBACK(on_select_all), app, "<Control>A", accel);
    add_item(edit_menu, "Time/&Date", G_CALLBACK(on_insert_time), app, "F5", accel);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), edit_item);

    GtkWidget *fmt_item = gtk_menu_item_new_with_mnemonic("F_ormat");
    GtkWidget *fmt_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(fmt_item), fmt_menu);
    app->menu_wordwrap = gtk_check_menu_item_new_with_mnemonic("_Word Wrap");
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(app->menu_wordwrap), TRUE);
    g_signal_connect(app->menu_wordwrap, "toggled", G_CALLBACK(on_word_wrap), app);
    gtk_menu_shell_append(GTK_MENU_SHELL(fmt_menu), app->menu_wordwrap);
    add_item(fmt_menu, "_Font...", G_CALLBACK(on_font), app, NULL, NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), fmt_item);

    GtkWidget *view_item = gtk_menu_item_new_with_mnemonic("_View");
    GtkWidget *view_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(view_item), view_menu);
    app->menu_statusbar = gtk_check_menu_item_new_with_mnemonic("_Status Bar");
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(app->menu_statusbar), TRUE);
    g_signal_connect(app->menu_statusbar, "toggled", G_CALLBACK(on_view_status), app);
    gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), app->menu_statusbar);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), view_item);

    GtkWidget *help_item = gtk_menu_item_new_with_mnemonic("_Help");
    GtkWidget *help_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(help_item), help_menu);
    add_item(help_menu, "_View Help", G_CALLBACK(on_help_view), app, NULL, NULL);
    add_separator(help_menu);
    add_item(help_menu, "_About Linotepad", G_CALLBACK(on_about), app, NULL, NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), help_item);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);
    app->textview = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(app->textview), GTK_WRAP_WORD);
    gtk_container_add(GTK_CONTAINER(scroll), app->textview);
    app->buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app->textview));

    gtk_widget_add_events(app->textview, GDK_SCROLL_MASK);
    g_signal_connect(app->textview, "scroll-event", G_CALLBACK(on_scroll), app);

    apply_font_size(app);

    g_signal_connect(app->buffer, "changed", G_CALLBACK(buffer_changed_cb), app);
    g_signal_connect(app->buffer, "notify::cursor-position",
                      G_CALLBACK(cursor_moved_cb), app);

    app->statusbar_pos = gtk_statusbar_new();
    app->status_ctx = gtk_statusbar_get_context_id(
        GTK_STATUSBAR(app->statusbar_pos), "poscontext");
    gtk_box_pack_start(GTK_BOX(vbox), app->statusbar_pos, FALSE, FALSE, 0);

    if (path) {
        set_buffer_text_from_file(app, path);
    } else {
        update_title(app);
    }

    gtk_widget_show_all(app->window);
    update_status(app);
    return app->window;
}

int main(int argc, char **argv) {
    gtk_init(&argc, &argv);

    create_window(argc > 1 ? argv[1] : NULL);
    gtk_main();

    return 0;
}
