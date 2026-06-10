#include "raylib.h"
#include "flint.h"
#include "sqlite3.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define UKU_MIN(a, b) ((a) < (b) ? (a) : (b))
#define UKU_MAX(a, b) ((a) > (b) ? (a) : (b))

typedef enum UkuScreen {
    UKU_SCREEN_HOME,
    UKU_SCREEN_CREATE,
    UKU_SCREEN_COLLECT,
    UKU_SCREEN_MANUAL
} UkuScreen;

typedef enum UkuField {
    UKU_FIELD_NONE,
    UKU_FIELD_TOPIC,
    UKU_FIELD_DESCRIPTION
} UkuField;

typedef struct UkuDecision {
    char id[40];
    char local_address[96];
    char topic[180];
    char description[420];
    int proposal_days;
    int proposal_hours;
    int proposal_minutes;
    int voting_days;
    int voting_hours;
    int voting_minutes;
    int negative_weight;
    int submitted;
    int topic_error;
    int db_error;
} UkuDecision;

#define UKU_MAX_PROCESSES 64

typedef struct UkuProcessRow {
    char id[40];
    char local_address[96];
    char topic[180];
    char description[420];
    int proposal_minutes;
    int voting_minutes;
    int negative_weight;
    sqlite3_int64 created_at;
} UkuProcessRow;

typedef struct UkuApp {
    UkuScreen screen;
    UkuField active_field;
    int cursor_clickable;
    int create_scroll;
    int create_max_scroll;
    int create_scroll_dragging;
    int create_scroll_drag_offset;
    int create_scrollbar_visible;
    int dashboard_scroll;
    int dashboard_max_scroll;
    int dashboard_drag_scrollbar;
    int dashboard_scroll_drag_offset;
    int manual_scroll;
    int manual_max_scroll;
    int manual_drag_scrollbar;
    int manual_scroll_drag_offset;
    int negative_dropdown_open;
    int process_count;
    Font font;
    Texture2D font_shapes_texture;
    int locale_font_ready;
    sqlite3 *db;
    UkuDecision decision;
    UkuProcessRow processes[UKU_MAX_PROCESSES];
} UkuApp;

typedef struct UkuText {
    char app_title[64];
    char home_title[64];
    char home_subtitle[96];
    char home_summary[512];
    char start_process_button[64];
    char dashboard_empty[160];
    char dashboard_recent_label[96];
    char manual_title[96];
    char manual_body[1536];
    char create_title[96];
    char topic_question_label[96];
    char topic_question_placeholder[128];
    char topic_error[128];
    char description_label[96];
    char description_placeholder[128];
    char negative_weight_label[96];
    char negative_weight_options[10][96];
    char proposal_time_label[96];
    char voting_time_label[96];
    char days_label[32];
    char hours_label[32];
    char minutes_label[32];
    char create_process_button[96];
    char setup_ready[96];
    char collect_title[96];
    char collect_intro[256];
    char local_address_label[96];
    char default_proposals_label[96];
    char status_quo_title[96];
    char status_quo_description[128];
    char repeat_process_title[96];
    char repeat_process_description[128];
    char db_error[128];
    char back_button[64];
    char brand_short[64];
} UkuText;

typedef enum UkuFocusId {
    UKU_FOCUS_HOME_START = 1,
    UKU_FOCUS_CREATE_BACK,
    UKU_FOCUS_TOPIC,
    UKU_FOCUS_DESCRIPTION,
    UKU_FOCUS_NEGATIVE_WEIGHT,
    UKU_FOCUS_PROPOSAL_DAYS_MINUS,
    UKU_FOCUS_PROPOSAL_DAYS_PLUS,
    UKU_FOCUS_PROPOSAL_HOURS_MINUS,
    UKU_FOCUS_PROPOSAL_HOURS_PLUS,
    UKU_FOCUS_PROPOSAL_MINUTES_MINUS,
    UKU_FOCUS_PROPOSAL_MINUTES_PLUS,
    UKU_FOCUS_VOTING_DAYS_MINUS,
    UKU_FOCUS_VOTING_DAYS_PLUS,
    UKU_FOCUS_VOTING_HOURS_MINUS,
    UKU_FOCUS_VOTING_HOURS_PLUS,
    UKU_FOCUS_VOTING_MINUTES_MINUS,
    UKU_FOCUS_VOTING_MINUTES_PLUS,
    UKU_FOCUS_CREATE_SUBMIT,
    UKU_FOCUS_COLLECT_BACK,
    UKU_FOCUS_DASHBOARD_MANUAL,
    UKU_FOCUS_DASHBOARD_NEW,
    UKU_FOCUS_MANUAL_BACK,
    UKU_FOCUS_DASHBOARD_PROCESS_BASE = 100
} UkuFocusId;

typedef struct ChoppedGlyph {
    int32_t value;
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
    int32_t offsetX;
    int32_t offsetY;
    int32_t advanceX;
} ChoppedGlyph;

static const Color C_BG = {250, 248, 242, 255};
static const Color C_TEXT = {31, 39, 51, 255};
static const Color C_MUTED = {91, 101, 115, 255};
static const Color C_LINE = {209, 214, 219, 255};
static const Color C_GREEN = {45, 121, 93, 255};
static const Color C_BLUE = {42, 92, 156, 255};
static const Color C_RED = {178, 73, 68, 255};

#define LOCALE_FONT_PNG "assets/fonts/locales.png"
#define LOCALE_FONT_DAT "assets/fonts/locales.dat"
#define LOCALE_TEXT_PATH "locales/en.txt"
#define LOCALE_FONT_BASE_SIZE 16
#define UKU_PACKAGE_ID "xyz.waozi.uku"

static void
copy_text(char *dst, size_t dst_size, const char *src, size_t len)
{
    size_t n;

    if(dst == NULL || dst_size == 0)
        return;

    while(len > 0 && (src[len - 1] == '\n' || src[len - 1] == '\r'))
        len--;
    n = UKU_MIN(len, dst_size - 1);
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static void
set_default_text(UkuText *text)
{
    memset(text, 0, sizeof(*text));
}

static void
assign_text(UkuText *text, const char *key, const char *value, size_t len)
{
    if(strcmp(key, "app_title") == 0)
        copy_text(text->app_title, sizeof(text->app_title), value, len);
    else if(strcmp(key, "home_title") == 0)
        copy_text(text->home_title, sizeof(text->home_title), value, len);
    else if(strcmp(key, "home_subtitle") == 0)
        copy_text(text->home_subtitle, sizeof(text->home_subtitle), value, len);
    else if(strcmp(key, "home_summary") == 0)
        copy_text(text->home_summary, sizeof(text->home_summary), value, len);
    else if(strcmp(key, "start_process_button") == 0)
        copy_text(text->start_process_button, sizeof(text->start_process_button), value, len);
    else if(strcmp(key, "dashboard_empty") == 0)
        copy_text(text->dashboard_empty, sizeof(text->dashboard_empty), value, len);
    else if(strcmp(key, "dashboard_recent_label") == 0)
        copy_text(text->dashboard_recent_label, sizeof(text->dashboard_recent_label), value, len);
    else if(strcmp(key, "manual_title") == 0)
        copy_text(text->manual_title, sizeof(text->manual_title), value, len);
    else if(strcmp(key, "manual_body") == 0)
        copy_text(text->manual_body, sizeof(text->manual_body), value, len);
    else if(strcmp(key, "create_title") == 0)
        copy_text(text->create_title, sizeof(text->create_title), value, len);
    else if(strcmp(key, "topic_question_label") == 0)
        copy_text(text->topic_question_label, sizeof(text->topic_question_label), value, len);
    else if(strcmp(key, "topic_question_placeholder") == 0)
        copy_text(text->topic_question_placeholder, sizeof(text->topic_question_placeholder), value, len);
    else if(strcmp(key, "topic_error") == 0)
        copy_text(text->topic_error, sizeof(text->topic_error), value, len);
    else if(strcmp(key, "description_label") == 0)
        copy_text(text->description_label, sizeof(text->description_label), value, len);
    else if(strcmp(key, "description_placeholder") == 0)
        copy_text(text->description_placeholder, sizeof(text->description_placeholder), value, len);
    else if(strcmp(key, "negative_weight_label") == 0)
        copy_text(text->negative_weight_label, sizeof(text->negative_weight_label), value, len);
    else if(strncmp(key, "negative_weight_", 16) == 0) {
        if(strcmp(key + 16, "infinity") == 0)
            copy_text(text->negative_weight_options[0], sizeof(text->negative_weight_options[0]), value, len);
        else {
            int index = atoi(key + 16);
            if(index >= 1 && index <= 9)
                copy_text(text->negative_weight_options[index], sizeof(text->negative_weight_options[index]), value, len);
        }
    }
    else if(strcmp(key, "proposal_time_label") == 0)
        copy_text(text->proposal_time_label, sizeof(text->proposal_time_label), value, len);
    else if(strcmp(key, "voting_time_label") == 0)
        copy_text(text->voting_time_label, sizeof(text->voting_time_label), value, len);
    else if(strcmp(key, "days_label") == 0)
        copy_text(text->days_label, sizeof(text->days_label), value, len);
    else if(strcmp(key, "hours_label") == 0)
        copy_text(text->hours_label, sizeof(text->hours_label), value, len);
    else if(strcmp(key, "minutes_label") == 0)
        copy_text(text->minutes_label, sizeof(text->minutes_label), value, len);
    else if(strcmp(key, "create_process_button") == 0)
        copy_text(text->create_process_button, sizeof(text->create_process_button), value, len);
    else if(strcmp(key, "setup_ready") == 0)
        copy_text(text->setup_ready, sizeof(text->setup_ready), value, len);
    else if(strcmp(key, "collect_title") == 0)
        copy_text(text->collect_title, sizeof(text->collect_title), value, len);
    else if(strcmp(key, "collect_intro") == 0)
        copy_text(text->collect_intro, sizeof(text->collect_intro), value, len);
    else if(strcmp(key, "local_address_label") == 0)
        copy_text(text->local_address_label, sizeof(text->local_address_label), value, len);
    else if(strcmp(key, "default_proposals_label") == 0)
        copy_text(text->default_proposals_label, sizeof(text->default_proposals_label), value, len);
    else if(strcmp(key, "status_quo_title") == 0)
        copy_text(text->status_quo_title, sizeof(text->status_quo_title), value, len);
    else if(strcmp(key, "status_quo_description") == 0)
        copy_text(text->status_quo_description, sizeof(text->status_quo_description), value, len);
    else if(strcmp(key, "repeat_process_title") == 0)
        copy_text(text->repeat_process_title, sizeof(text->repeat_process_title), value, len);
    else if(strcmp(key, "repeat_process_description") == 0)
        copy_text(text->repeat_process_description, sizeof(text->repeat_process_description), value, len);
    else if(strcmp(key, "db_error") == 0)
        copy_text(text->db_error, sizeof(text->db_error), value, len);
    else if(strcmp(key, "back_button") == 0)
        copy_text(text->back_button, sizeof(text->back_button), value, len);
    else if(strcmp(key, "brand_short") == 0)
        copy_text(text->brand_short, sizeof(text->brand_short), value, len);
}

static void
load_text_file(UkuText *text, const char *path)
{
    char *data = LoadFileText(path);
    char key[96] = {0};
    char *line;
    char *value_start = NULL;

    set_default_text(text);
    if(data == NULL)
        return;

    line = data;
    while(line != NULL && *line != '\0') {
        char *next = strchr(line, '\n');
        size_t line_len = next == NULL ? strlen(line) : (size_t)(next - line);

        if(line_len > 0 && line[line_len - 1] == '\r')
            line_len--;

        if(line_len >= 3 && strncmp(line, "---", 3) == 0) {
            if(key[0] != '\0' && value_start != NULL)
                assign_text(text, key, value_start, (size_t)(line - value_start));
            key[0] = '\0';
            value_start = NULL;
        } else if(line_len >= 3 && line[0] == '[' && line[line_len - 1] == ']') {
            size_t key_len = UKU_MIN(line_len - 2, sizeof(key) - 1);
            memcpy(key, line + 1, key_len);
            key[key_len] = '\0';
            value_start = next == NULL ? NULL : next + 1;
        }

        if(next == NULL)
            break;
        line = next + 1;
    }

    if(key[0] != '\0' && value_start != NULL)
        assign_text(text, key, value_start, strlen(value_start));

    UnloadFileText(data);
}

static Font
load_chopped_font(const char *png_path, const char *dat_path)
{
    Font font = {0};
    FILE *file = NULL;
    ChoppedGlyph *glyphs = NULL;
    GlyphInfo *glyph_infos = NULL;
    Rectangle *recs = NULL;
    int32_t glyph_count = 0;
    Image image = {0};
    Texture2D texture = {0};

    file = fopen(dat_path, "rb");
    if(file == NULL)
        return font;

    if(fread(&glyph_count, sizeof(glyph_count), 1, file) != 1 || glyph_count <= 0) {
        fclose(file);
        return font;
    }

    glyphs = calloc((size_t)glyph_count, sizeof(*glyphs));
    glyph_infos = calloc((size_t)glyph_count, sizeof(*glyph_infos));
    recs = calloc((size_t)glyph_count, sizeof(*recs));
    if(glyphs == NULL || glyph_infos == NULL || recs == NULL)
        goto cleanup;

    if(fread(glyphs, sizeof(*glyphs), (size_t)glyph_count, file) != (size_t)glyph_count)
        goto cleanup;
    fclose(file);
    file = NULL;

    image = LoadImage(png_path);
    if(image.data == NULL)
        goto cleanup;

    texture = LoadTextureFromImage(image);
    UnloadImage(image);
    image = (Image){0};
    if(texture.id == 0)
        goto cleanup;
    SetTextureFilter(texture, TEXTURE_FILTER_POINT);

    for(int i = 0; i < glyph_count; i++) {
        glyph_infos[i].value = glyphs[i].value;
        glyph_infos[i].offsetX = glyphs[i].offsetX;
        glyph_infos[i].offsetY = glyphs[i].offsetY;
        glyph_infos[i].advanceX = glyphs[i].advanceX;
        glyph_infos[i].image = (Image){0};

        recs[i].x = (float)glyphs[i].x;
        recs[i].y = (float)glyphs[i].y;
        recs[i].width = (float)glyphs[i].w;
        recs[i].height = (float)glyphs[i].h;
    }

    font.texture = texture;
    font.glyphs = glyph_infos;
    font.recs = recs;
    font.glyphCount = glyph_count;
    font.baseSize = LOCALE_FONT_BASE_SIZE;
    font.glyphPadding = 0;

    free(glyphs);
    return font;

cleanup:
    if(file != NULL)
        fclose(file);
    if(image.data != NULL)
        UnloadImage(image);
    if(texture.id != 0)
        UnloadTexture(texture);
    free(glyphs);
    free(glyph_infos);
    free(recs);
    return (Font){0};
}

static void
app_load_font(UkuApp *app)
{
    Image white;

    app->font = load_chopped_font(LOCALE_FONT_PNG, LOCALE_FONT_DAT);
    if(app->font.texture.id == 0) {
        app->font = GetFontDefault();
        app->locale_font_ready = 0;
        return;
    }

    white = GenImageColor(1, 1, WHITE);
    app->font_shapes_texture = LoadTextureFromImage(white);
    UnloadImage(white);
    if(app->font_shapes_texture.id != 0)
        SetShapesTexture(app->font_shapes_texture, (Rectangle){0, 0, 1, 1});
    app->locale_font_ready = 1;
}

static void
app_unload_font(UkuApp *app)
{
    if(app->locale_font_ready) {
        UnloadTexture(app->font.texture);
        free(app->font.glyphs);
        free(app->font.recs);
    }
    if(app->font_shapes_texture.id != 0)
        UnloadTexture(app->font_shapes_texture);
}

static int
measure_text_font(Font font, const char *text, int font_size)
{
    return (int)(MeasureTextEx(font, text, (float)font_size, 0).x + 0.5f);
}

static void
draw_text_font(Font font, const char *text, int x, int y, int font_size, Color color)
{
    DrawTextEx(font, text, (Vector2){(float)x, (float)y}, (float)font_size, 0, color);
}

static int
draw_wrapped_text(Font font, const char *text, int x, int y, int w, int font_size, int line_h, Color color)
{
    const char *p = text;
    char line[512];
    int line_len = 0;
    int line_w = 0;
    int space_w = measure_text_font(font, " ", font_size);

    line[0] = '\0';
    while(*p != '\0') {
        char word[160];
        int word_len = 0;
        int forced_break = 0;

        while(*p == ' ')
            p++;
        if(*p == '\n') {
            forced_break = 1;
            p++;
        }

        while(*p != '\0' && *p != ' ' && *p != '\n' && word_len < (int)sizeof(word) - 1)
            word[word_len++] = *p++;
        word[word_len] = '\0';

        if(forced_break || word_len == 0) {
            if(line_len > 0) {
                draw_text_font(font, line, x, y, font_size, color);
                y += line_h;
                line_len = 0;
                line_w = 0;
                line[0] = '\0';
            }
            continue;
        }

        int word_w = measure_text_font(font, word, font_size);
        int add_w = line_len == 0 ? word_w : space_w + word_w;
        if(line_len > 0 && line_w + add_w > w) {
            draw_text_font(font, line, x, y, font_size, color);
            y += line_h;
            line_len = 0;
            line_w = 0;
            line[0] = '\0';
        }

        if(line_len == 0) {
            copy_text(line, sizeof(line), word, (size_t)word_len);
            line_len = (int)strlen(line);
            line_w = word_w;
        } else if(line_len + 1 + word_len < (int)sizeof(line)) {
            line[line_len++] = ' ';
            memcpy(line + line_len, word, (size_t)word_len + 1);
            line_len += word_len;
            line_w += add_w;
        }
    }

    if(line_len > 0) {
        draw_text_font(font, line, x, y, font_size, color);
        y += line_h;
    }

    return y;
}

static void
draw_centered_text(Font font, const char *text, int center_x, int y, int font_size, Color color)
{
    draw_text_font(font, text, center_x - measure_text_font(font, text, font_size) / 2, y, font_size, color);
}

static int
clampi(int value, int min_value, int max_value)
{
    if(value < min_value)
        return min_value;
    if(value > max_value)
        return max_value;
    return value;
}

static int
has_non_space(const char *text)
{
    for(const char *p = text; *p != '\0'; p++) {
        if(*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r')
            return 1;
    }
    return 0;
}

static int
exec_sql(sqlite3 *db, const char *sql)
{
    char *error = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &error);

    if(error != NULL)
        sqlite3_free(error);
    return rc == SQLITE_OK;
}

static int
db_init(UkuApp *app)
{
    if(sqlite3_open("uku.sqlite3", &app->db) != SQLITE_OK)
        return 0;

    return exec_sql(app->db,
                    "create table if not exists processes ("
                    "id text primary key,"
                    "topic text not null,"
                    "description text not null,"
                    "proposal_minutes integer not null,"
                    "voting_minutes integer not null,"
                    "negative_weight integer not null,"
                    "local_address text not null,"
                    "created_at integer not null"
                    ");"
                    "create table if not exists proposals ("
                    "id integer primary key autoincrement,"
                    "process_id text not null,"
                    "title text not null,"
                    "description text not null,"
                    "created_at integer not null,"
                    "foreign key(process_id) references processes(id)"
                    ");");
}

static int
duration_minutes(int days, int hours, int minutes)
{
    return days * 24 * 60 + hours * 60 + minutes;
}

static void
generate_process_id(char *dst, size_t size)
{
    unsigned int a = (unsigned int)time(NULL);
    unsigned int b = (unsigned int)GetRandomValue(0, 0x7fffffff);

    snprintf(dst, size, "%08x-%08x", a, b);
}

static int
db_insert_proposal(sqlite3 *db, const char *process_id, const char *title, const char *description, sqlite3_int64 created_at)
{
    sqlite3_stmt *stmt = NULL;
    int ok;

    if(sqlite3_prepare_v2(db,
                          "insert into proposals(process_id, title, description, created_at) values(?, ?, ?, ?)",
                          -1, &stmt, NULL) != SQLITE_OK)
        return 0;

    sqlite3_bind_text(stmt, 1, process_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, title, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, description, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, created_at);
    ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

static int
db_save_process(UkuApp *app, const UkuText *text)
{
    UkuDecision *d = &app->decision;
    sqlite3_stmt *stmt = NULL;
    sqlite3_int64 now = (sqlite3_int64)time(NULL);
    int ok = 0;

    if(app->db == NULL)
        return 0;

    generate_process_id(d->id, sizeof(d->id));
    snprintf(d->local_address, sizeof(d->local_address), "/app/%s/collect", d->id);

    if(sqlite3_exec(app->db, "begin immediate", NULL, NULL, NULL) != SQLITE_OK)
        return 0;

    if(sqlite3_prepare_v2(app->db,
                          "insert into processes(id, topic, description, proposal_minutes, voting_minutes, negative_weight, local_address, created_at)"
                          " values(?, ?, ?, ?, ?, ?, ?, ?)",
                          -1, &stmt, NULL) != SQLITE_OK)
        goto cleanup;

    sqlite3_bind_text(stmt, 1, d->id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, d->topic, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, d->description, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, duration_minutes(d->proposal_days, d->proposal_hours, d->proposal_minutes));
    sqlite3_bind_int(stmt, 5, duration_minutes(d->voting_days, d->voting_hours, d->voting_minutes));
    sqlite3_bind_int(stmt, 6, d->negative_weight);
    sqlite3_bind_text(stmt, 7, d->local_address, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 8, now);

    if(sqlite3_step(stmt) != SQLITE_DONE)
        goto cleanup;
    sqlite3_finalize(stmt);
    stmt = NULL;

    if(!db_insert_proposal(app->db, d->id, text->status_quo_title, text->status_quo_description, now))
        goto cleanup;
    if(!db_insert_proposal(app->db, d->id, text->repeat_process_title, text->repeat_process_description, now))
        goto cleanup;

    ok = sqlite3_exec(app->db, "commit", NULL, NULL, NULL) == SQLITE_OK;

cleanup:
    if(stmt != NULL)
        sqlite3_finalize(stmt);
    if(!ok)
        sqlite3_exec(app->db, "rollback", NULL, NULL, NULL);
    return ok;
}

static void
db_load_processes(UkuApp *app)
{
    sqlite3_stmt *stmt = NULL;
    int count = 0;

    app->process_count = 0;
    if(app->db == NULL)
        return;

    if(sqlite3_prepare_v2(app->db,
                          "select id, topic, description, proposal_minutes, voting_minutes, negative_weight, local_address, created_at "
                          "from processes order by created_at desc limit ?",
                          -1, &stmt, NULL) != SQLITE_OK)
        return;

    sqlite3_bind_int(stmt, 1, UKU_MAX_PROCESSES);
    while(count < UKU_MAX_PROCESSES && sqlite3_step(stmt) == SQLITE_ROW) {
        UkuProcessRow *row = &app->processes[count];
        const unsigned char *id = sqlite3_column_text(stmt, 0);
        const unsigned char *topic = sqlite3_column_text(stmt, 1);
        const unsigned char *description = sqlite3_column_text(stmt, 2);
        const unsigned char *address = sqlite3_column_text(stmt, 6);

        copy_text(row->id, sizeof(row->id), (const char *)(id != NULL ? id : (const unsigned char *)""), strlen((const char *)(id != NULL ? id : (const unsigned char *)"")));
        copy_text(row->topic, sizeof(row->topic), (const char *)(topic != NULL ? topic : (const unsigned char *)""), strlen((const char *)(topic != NULL ? topic : (const unsigned char *)"")));
        copy_text(row->description, sizeof(row->description), (const char *)(description != NULL ? description : (const unsigned char *)""), strlen((const char *)(description != NULL ? description : (const unsigned char *)"")));
        copy_text(row->local_address, sizeof(row->local_address), (const char *)(address != NULL ? address : (const unsigned char *)""), strlen((const char *)(address != NULL ? address : (const unsigned char *)"")));
        row->proposal_minutes = sqlite3_column_int(stmt, 3);
        row->voting_minutes = sqlite3_column_int(stmt, 4);
        row->negative_weight = sqlite3_column_int(stmt, 5);
        row->created_at = sqlite3_column_int64(stmt, 7);
        count++;
    }

    app->process_count = count;
    sqlite3_finalize(stmt);
}

static void
open_process_row(UkuApp *app, const UkuProcessRow *row)
{
    UkuDecision *d = &app->decision;

    memset(d, 0, sizeof(*d));
    copy_text(d->id, sizeof(d->id), row->id, strlen(row->id));
    copy_text(d->local_address, sizeof(d->local_address), row->local_address, strlen(row->local_address));
    copy_text(d->topic, sizeof(d->topic), row->topic, strlen(row->topic));
    copy_text(d->description, sizeof(d->description), row->description, strlen(row->description));
    d->proposal_days = row->proposal_minutes / (24 * 60);
    d->proposal_hours = (row->proposal_minutes / 60) % 24;
    d->proposal_minutes = row->proposal_minutes % 60;
    d->voting_days = row->voting_minutes / (24 * 60);
    d->voting_hours = (row->voting_minutes / 60) % 24;
    d->voting_minutes = row->voting_minutes % 60;
    d->negative_weight = row->negative_weight;
    d->submitted = 1;
    app->screen = UKU_SCREEN_COLLECT;
    app->active_field = UKU_FIELD_NONE;
    ui_focus_clear();
}

static void
append_char(char *buffer, size_t cap, int codepoint)
{
    size_t len = strlen(buffer);

    if(codepoint < 32 || codepoint > 126 || len + 1 >= cap)
        return;

    buffer[len] = (char)codepoint;
    buffer[len + 1] = '\0';
}

static void
delete_char(char *buffer)
{
    size_t len = strlen(buffer);

    if(len > 0)
        buffer[len - 1] = '\0';
}

static const char *
fit_tail(Font font, const char *text, int font_size, int w)
{
    const char *p = text;

    while(*p != '\0' && measure_text_font(font, p, font_size) > w)
        p++;

    return p;
}

static void
draw_button(UkuApp *app, Font font, int x, int y, int w, int h, const char *label, int primary, int focus_id, int *clicked)
{
    Vector2 mouse = GetMousePosition();
    Rectangle bounds = {(float)x, (float)y, (float)w, (float)h};
    int hover = CheckCollisionPointRec(mouse, bounds);
    int focused = ui_focus_register(focus_id, bounds);
    Color fill = primary ? C_BLUE : (Color){255, 255, 255, 255};
    Color text = primary ? WHITE : C_TEXT;
    int font_size = flint_clamp_px(16, 16, 20);

    if(hover || focused) {
        fill = primary ? flint_lighten(C_BLUE, 18) : (Color){242, 245, 247, 255};
        if(hover)
            app->cursor_clickable = 1;
    }

    DrawRectangleRounded(bounds, 0.12f, 12, fill);
    DrawRectangleRoundedLinesEx(bounds, 0.12f, 12, flint_px(1), primary ? flint_darken(C_BLUE, 20) : C_LINE);
    if(focused)
        ui_focus_draw(bounds);
    draw_centered_text(font, label, x + w / 2, y + (h - font_size) / 2, font_size, text);

    *clicked = (hover && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) || ui_focus_activate_pressed(focus_id);
}

static int
draw_icon_button(UkuApp *app, Font font, int x, int y, int size, const char *label,
                 FlintIconType icon, int focus_id, int primary)
{
    Rectangle bounds = {(float)x, (float)y, (float)size, (float)size};
    Vector2 mouse = GetMousePosition();
    int hover = CheckCollisionPointRec(mouse, bounds);
    int focused = ui_focus_register(focus_id, bounds);
    int clicked;
    Color fill = primary ? C_BLUE : WHITE;
    Color ink = primary ? WHITE : C_TEXT;
    int font_size = flint_clamp_px(20, 18, 24);

    if(hover || focused)
        fill = primary ? flint_lighten(C_BLUE, 18) : (Color){242, 245, 247, 255};
    if(hover)
        app->cursor_clickable = 1;

    DrawRectangleRounded(bounds, 0.22f, 12, fill);
    DrawRectangleRoundedLinesEx(bounds, 0.22f, 12, flint_px(1), primary ? flint_darken(C_BLUE, 20) : C_LINE);
    if(focused)
        ui_focus_draw(bounds);

    if(icon != FLINT_ICON_TYPE_NONE)
        flint_draw_icon_fallback(icon, x + size / 2 - flint_px(11), y + size / 2 - flint_px(11), flint_px(22), ink);
    else
        draw_centered_text(font, label, x + size / 2, y + (size - font_size) / 2, font_size, ink);

    clicked = (hover && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) || ui_focus_activate_pressed(focus_id);
    return clicked;
}

static void
draw_top_bar(UkuApp *app, Font font, const char *title, int show_back, int back_focus_id, int *back_clicked,
             int show_manual, int *manual_clicked, int view_w)
{
    int h = flint_px(58);
    int font_size = flint_clamp_px(18, 18, 24);
    int x = flint_px(18);

    DrawRectangle(0, 0, view_w, h, (Color){246, 243, 235, 255});
    DrawLine(0, h - 1, view_w, h - 1, C_LINE);

    if(show_back) {
        int clicked = draw_icon_button(app, font, x, flint_px(9), flint_px(40), "<",
                                       FLINT_ICON_TYPE_RETURN, back_focus_id, 0);
        if(back_clicked != NULL && clicked)
            *back_clicked = 1;
        x += flint_px(52);
    }

    draw_text_font(font, title, x, flint_ui_text_y(title, 0, h, font_size), font_size, C_TEXT);

    if(show_manual && manual_clicked != NULL) {
        int clicked = draw_icon_button(app, font, view_w - flint_px(58), flint_px(9), flint_px(40), "?",
                                       FLINT_ICON_TYPE_MANUAL, UKU_FOCUS_DASHBOARD_MANUAL, 0);
        if(clicked)
            *manual_clicked = 1;
    }
}

static int
draw_text_field(UkuApp *app, Font font, const char *label, const char *placeholder,
                char *buffer, size_t cap, UkuField field, int focus_id, int x, int y, int w, int h)
{
    int label_font = flint_clamp_px(13, 13, 16);
    int input_font = flint_clamp_px(16, 16, 20);
    int pad = flint_px(12);
    int label_y = y;
    int box_y = y + label_font + flint_px(8);
    Rectangle box = {(float)x, (float)box_y, (float)w, (float)h};
    Vector2 mouse = GetMousePosition();
    int focused;
    int active;

    draw_text_font(font, label, x, label_y, label_font, C_MUTED);

    if(IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if(CheckCollisionPointRec(mouse, box))
            app->active_field = field;
        else if(app->active_field == field)
            app->active_field = UKU_FIELD_NONE;
    }

    focused = ui_focus_register(focus_id, box);
    if(focused)
        app->active_field = field;
    active = app->active_field == field;
    if(CheckCollisionPointRec(mouse, box))
        app->cursor_clickable = 1;

    if(active) {
        int c = GetCharPressed();
        while(c > 0) {
            append_char(buffer, cap, c);
            c = GetCharPressed();
        }
        if(IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE))
            delete_char(buffer);
    }

    DrawRectangleRounded(box, 0.08f, 10, WHITE);
    DrawRectangleRoundedLinesEx(box, 0.08f, 10, flint_px(active ? 2 : 1),
                                active ? C_BLUE : C_LINE);
    if(focused)
        ui_focus_draw(box);

    if(buffer[0] == '\0') {
        draw_text_font(font, placeholder, x + pad, box_y + pad, input_font, (Color){142, 149, 160, 255});
    } else if(h > flint_px(58)) {
        BeginScissorMode(x + pad, box_y + pad, w - pad * 2, h - pad * 2);
        draw_wrapped_text(font, buffer, x + pad, box_y + pad, w - pad * 2,
                          input_font, input_font + flint_px(7), C_TEXT);
        EndScissorMode();
    } else {
        const char *visible = fit_tail(font, buffer, input_font, w - pad * 2 - flint_px(8));
        draw_text_font(font, visible, x + pad, box_y + (h - input_font) / 2, input_font, C_TEXT);
    }

    if(active && ((int)(GetTime() * 2.0) % 2) == 0) {
        int cursor_x;
        if(h > flint_px(58))
            cursor_x = x + pad + UKU_MIN(measure_text_font(font, buffer, input_font), w - pad * 2 - flint_px(4));
        else
            cursor_x = x + pad + measure_text_font(font, fit_tail(font, buffer, input_font, w - pad * 2 - flint_px(8)), input_font);
        DrawLine(cursor_x, box_y + pad, cursor_x, box_y + h - pad, C_BLUE);
    }

    return box_y + h + flint_px(16);
}

static int
draw_stepper(UkuApp *app, Font font, const char *label, int *value, int min_value, int max_value,
             int x, int y, int w, int minus_focus_id, int plus_focus_id)
{
    int label_font = flint_clamp_px(13, 13, 16);
    int value_font = flint_clamp_px(18, 18, 22);
    int btn = flint_px(34);
    int h = flint_px(38);
    int value_w = w - btn * 2 - flint_px(8);
    int minus_clicked = 0;
    int plus_clicked = 0;
    char value_text[16];

    draw_text_font(font, label, x, y, label_font, C_MUTED);
    y += label_font + flint_px(7);

    draw_button(app, font, x, y, btn, h, "-", 0, minus_focus_id, &minus_clicked);
    DrawRectangleRounded((Rectangle){x + btn + flint_px(4), y, value_w, h}, 0.08f, 10, WHITE);
    DrawRectangleRoundedLinesEx((Rectangle){x + btn + flint_px(4), y, value_w, h}, 0.08f, 10, flint_px(1), C_LINE);
    snprintf(value_text, sizeof(value_text), "%d", *value);
    draw_centered_text(font, value_text, x + btn + flint_px(4) + value_w / 2,
                       y + (h - value_font) / 2, value_font, C_TEXT);
    draw_button(app, font, x + btn + flint_px(8) + value_w, y, btn, h, "+", 0, plus_focus_id, &plus_clicked);

    if(minus_clicked)
        *value = clampi(*value - 1, min_value, max_value);
    if(plus_clicked)
        *value = clampi(*value + 1, min_value, max_value);

    return y + h + flint_px(14);
}

static int
draw_negative_weight_dropdown(UkuApp *app, Font font, const UkuText *text, int x, int y, int w, int focus_id)
{
    int label_font = flint_clamp_px(13, 13, 16);
    int input_font = flint_clamp_px(16, 16, 20);
    int h = flint_px(40);
    int option_h = flint_px(34);
    int pad = flint_px(12);
    int box_y;
    Rectangle box;
    Vector2 mouse = GetMousePosition();
    int selected = clampi(app->decision.negative_weight, 0, 9);
    int focused;

    draw_text_font(font, text->negative_weight_label, x, y, label_font, C_MUTED);
    box_y = y + label_font + flint_px(8);
    box = (Rectangle){(float)x, (float)box_y, (float)w, (float)h};
    focused = ui_focus_register(focus_id, box);

    if(CheckCollisionPointRec(mouse, box))
        app->cursor_clickable = 1;
    if((CheckCollisionPointRec(mouse, box) && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) ||
       ui_focus_activate_pressed(focus_id)) {
        app->negative_dropdown_open = !app->negative_dropdown_open;
        app->active_field = UKU_FIELD_NONE;
    }
    if(focused) {
        if(IsKeyPressed(KEY_DOWN))
            app->decision.negative_weight = clampi(app->decision.negative_weight + 1, 0, 9);
        if(IsKeyPressed(KEY_UP))
            app->decision.negative_weight = clampi(app->decision.negative_weight - 1, 0, 9);
    }

    DrawRectangleRounded(box, 0.08f, 10, WHITE);
    DrawRectangleRoundedLinesEx(box, 0.08f, 10, flint_px(1), app->negative_dropdown_open ? C_BLUE : C_LINE);
    if(focused)
        ui_focus_draw(box);
    draw_text_font(font, text->negative_weight_options[selected], x + pad,
                   box_y + (h - input_font) / 2, input_font, C_TEXT);
    DrawTriangle((Vector2){(float)(x + w - pad - flint_px(10)), (float)(box_y + h / 2 - flint_px(3))},
                 (Vector2){(float)(x + w - pad), (float)(box_y + h / 2 - flint_px(3))},
                 (Vector2){(float)(x + w - pad - flint_px(5)), (float)(box_y + h / 2 + flint_px(4))},
                 C_MUTED);

    if(app->negative_dropdown_open) {
        int menu_y = box_y + h + flint_px(4);
        int menu_h = option_h * 10;
        Rectangle menu = {(float)x, (float)menu_y, (float)w, (float)menu_h};

        DrawRectangleRounded(menu, 0.06f, 10, WHITE);
        DrawRectangleRoundedLinesEx(menu, 0.06f, 10, flint_px(1), C_LINE);
        for(int i = 0; i < 10; i++) {
            int oy = menu_y + i * option_h;
            Rectangle option = {(float)x, (float)oy, (float)w, (float)option_h};
            int hover = CheckCollisionPointRec(mouse, option);

            if(hover) {
                DrawRectangle(x + flint_px(2), oy, w - flint_px(4), option_h, (Color){238, 243, 247, 255});
                app->cursor_clickable = 1;
            }
            if(i == selected)
                DrawRectangle(x + flint_px(5), oy + flint_px(8), flint_px(4), option_h - flint_px(16), C_BLUE);
            draw_text_font(font, text->negative_weight_options[i], x + pad + flint_px(8),
                           oy + (option_h - input_font) / 2, input_font, C_TEXT);
            if(hover && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
                app->decision.negative_weight = i;
                app->negative_dropdown_open = 0;
            }
        }

        if(IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
           !CheckCollisionPointRec(mouse, box) && !CheckCollisionPointRec(mouse, menu))
            app->negative_dropdown_open = 0;

        return menu_y + menu_h + flint_px(18);
    }

    return box_y + h + flint_px(16);
}

static void
draw_scrollbar(UkuApp *app, int x, int y, int h, int content_h, int max_scroll,
               int *scroll, int *dragging, int *drag_offset)
{
    int track_w = flint_px(8);
    int thumb_h;
    int thumb_y;
    Rectangle track;
    Rectangle thumb;
    Vector2 mouse = GetMousePosition();

    track = (Rectangle){(float)x, (float)y, (float)track_w, (float)h};
    if(max_scroll <= 0) {
        *dragging = 0;
        return;
    }

    thumb_h = UKU_MAX(flint_px(42), (int)((float)h * (float)h / (float)content_h));
    thumb_h = UKU_MIN(thumb_h, h);
    thumb_y = y + (int)((float)(h - thumb_h) * ((float)(*scroll) / (float)max_scroll));
    thumb = (Rectangle){(float)x, (float)thumb_y, (float)track_w, (float)thumb_h};

    DrawRectangleRounded(track, 0.5f, 8, (Color){226, 230, 233, 255});
    DrawRectangleRounded(thumb, 0.5f, 8, C_BLUE);

    if(CheckCollisionPointRec(mouse, track))
        app->cursor_clickable = 1;

    if(IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mouse, thumb)) {
        *dragging = 1;
        *drag_offset = (int)mouse.y - thumb_y;
    }

    if(!IsMouseButtonDown(MOUSE_LEFT_BUTTON))
        *dragging = 0;

    if(*dragging) {
        int local_y = (int)mouse.y - y - *drag_offset;
        local_y = clampi(local_y, 0, h - thumb_h);
        *scroll = (int)((float)local_y / (float)(h - thumb_h) * (float)max_scroll + 0.5f);
        app->cursor_clickable = 1;
    }
}

static int
draw_duration_group(UkuApp *app, Font font, const char *title, const UkuText *text,
                    int *days, int *hours, int *minutes, int x, int y, int w, int focus_base)
{
    int title_font = flint_clamp_px(16, 16, 20);
    int gap = flint_px(10);
    int col_w = (w - gap * 2) / 3;
    int y2;

    draw_text_font(font, title, x, y, title_font, C_TEXT);
    y += title_font + flint_px(12);

    y2 = draw_stepper(app, font, text->days_label, days, 0, 30, x, y, col_w, focus_base, focus_base + 1);
    draw_stepper(app, font, text->hours_label, hours, 0, 23, x + col_w + gap, y, col_w, focus_base + 2, focus_base + 3);
    draw_stepper(app, font, text->minutes_label, minutes, 0, 59, x + (col_w + gap) * 2, y, col_w, focus_base + 4, focus_base + 5);
    return y2 + flint_px(6);
}

static void
init_decision(UkuApp *app, const UkuText *text)
{
    UkuDecision *d = &app->decision;

    d->proposal_days = 2;
    d->proposal_hours = 0;
    d->proposal_minutes = 1;
    d->voting_days = 1;
    d->voting_hours = 0;
    d->voting_minutes = 1;
    d->negative_weight = 3;
    (void)text;
}

static void
reset_decision(UkuApp *app, const UkuText *text)
{
    memset(&app->decision, 0, sizeof(app->decision));
    init_decision(app, text);
    app->create_scroll = 0;
    app->create_max_scroll = 0;
    app->create_scrollbar_visible = 0;
    app->negative_dropdown_open = 0;
    app->active_field = UKU_FIELD_NONE;
}

static void
format_created_at(char *dst, size_t size, sqlite3_int64 value)
{
    time_t t = (time_t)value;
    struct tm *tmv = localtime(&t);

    if(tmv == NULL) {
        copy_text(dst, size, "", 0);
        return;
    }
    strftime(dst, size, "%Y-%m-%d %H:%M", tmv);
}

static void
draw_home(UkuApp *app, const UkuText *text, int view_w, int view_h)
{
    int side = flint_page_side_padding();
    int content_x;
    int content_w;
    int top_h = flint_px(58);
    int title_font = flint_clamp_px(22, 22, 30);
    int body_font = flint_clamp_px(16, 16, 20);
    int small_font = flint_clamp_px(13, 13, 16);
    int line_h = body_font + flint_px(8);
    int viewport_y = top_h;
    int viewport_h = view_h - viewport_y;
    int y = viewport_y + flint_px(20) - app->dashboard_scroll;
    int content_bottom;
    int content_h;
    int manual_clicked = 0;
    int new_clicked = 0;
    Font font = app->font;

    db_load_processes(app);
    flint_centered_column(760, side, &content_x, &content_w);
    app->dashboard_scroll = clampi(app->dashboard_scroll - (int)(GetMouseWheelMove() * flint_px(44)),
                                   0, app->dashboard_max_scroll);

    draw_top_bar(app, font, text->home_title, 0, 0, NULL, 1, &manual_clicked, view_w);
    if(manual_clicked) {
        app->screen = UKU_SCREEN_MANUAL;
        app->manual_scroll = 0;
        ui_focus_clear();
    }

    BeginScissorMode(0, viewport_y, view_w, viewport_h);
    draw_text_font(font, text->dashboard_recent_label, content_x, y, title_font, C_TEXT);
    y += title_font + flint_px(18);

    if(app->process_count <= 0) {
        y = draw_wrapped_text(font, text->dashboard_empty, content_x, y, content_w, body_font, line_h, C_MUTED);
    } else {
        int card_h = flint_px(96);
        int gap = flint_px(12);
        for(int i = 0; i < app->process_count; i++) {
            UkuProcessRow *row = &app->processes[i];
            Rectangle card = {(float)content_x, (float)y, (float)content_w, (float)card_h};
            Vector2 mouse = GetMousePosition();
            int focus_id = UKU_FOCUS_DASHBOARD_PROCESS_BASE + i;
            int hovered = CheckCollisionPointRec(mouse, card);
            int focused = ui_focus_register(focus_id, card);
            char meta[160];
            char created[32];
            int open = 0;

            if(y + card_h < viewport_y || y > viewport_y + viewport_h) {
                y += card_h + gap;
                continue;
            }

            if(hovered)
                app->cursor_clickable = 1;
            DrawRectangleRounded(card, 0.07f, 10, hovered || focused ? (Color){244, 248, 250, 255} : WHITE);
            DrawRectangleRoundedLinesEx(card, 0.07f, 10, flint_px(1), focused ? C_BLUE : C_LINE);
            if(focused)
                ui_focus_draw(card);

            draw_text_font(font, fit_tail(font, row->topic, body_font, content_w - flint_px(28)),
                           content_x + flint_px(14), y + flint_px(12), body_font, C_TEXT);
            format_created_at(created, sizeof(created), row->created_at);
            snprintf(meta, sizeof(meta), "%s  |  %s", row->local_address, created);
            draw_text_font(font, fit_tail(font, meta, small_font, content_w - flint_px(28)),
                           content_x + flint_px(14), y + flint_px(42), small_font, C_GREEN);
            if(row->description[0] != '\0')
                draw_text_font(font, fit_tail(font, row->description, small_font, content_w - flint_px(28)),
                               content_x + flint_px(14), y + flint_px(66), small_font, C_MUTED);

            open = (hovered && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) || ui_focus_activate_pressed(focus_id);
            if(open)
                open_process_row(app, row);
            y += card_h + gap;
        }
    }
    EndScissorMode();

    content_bottom = y + app->dashboard_scroll + flint_px(24);
    content_h = content_bottom - viewport_y;
    app->dashboard_max_scroll = UKU_MAX(0, content_h - viewport_h);
    app->dashboard_scroll = clampi(app->dashboard_scroll, 0, app->dashboard_max_scroll);
    draw_scrollbar(app, view_w - side - flint_px(8), viewport_y + flint_px(8),
                   viewport_h - flint_px(16), content_h, app->dashboard_max_scroll,
                   &app->dashboard_scroll, &app->dashboard_drag_scrollbar, &app->dashboard_scroll_drag_offset);

    new_clicked = draw_icon_button(app, font, view_w - flint_px(70), view_h - flint_px(70),
                                   flint_px(52), "+", FLINT_ICON_TYPE_NONE, UKU_FOCUS_DASHBOARD_NEW, 1);
    if(new_clicked) {
        reset_decision(app, text);
        app->screen = UKU_SCREEN_CREATE;
        ui_focus_clear();
    }
}

static void
draw_create_placeholder(UkuApp *app, const UkuText *text, int view_w, int view_h)
{
    int side = flint_page_side_padding();
    int content_x;
    int content_w;
    int small_font = flint_clamp_px(13, 13, 16);
    int top_h = flint_px(58);
    int y = top_h + flint_px(20) - app->create_scroll;
    int start_y = y;
    int clicked = 0;
    int back_clicked = 0;
    Font font = app->font;
    UkuDecision *d = &app->decision;
    int max_scroll;
    int content_bottom;
    int content_h;
    int viewport_y = top_h;
    int viewport_h = view_h - viewport_y;
    int reserve_scrollbar = app->create_scrollbar_visible;

    app->create_scroll = clampi(app->create_scroll - (int)(GetMouseWheelMove() * flint_px(44)),
                                0, app->create_max_scroll);
    flint_centered_column(680, side, &content_x, &content_w);
    if(reserve_scrollbar)
        content_w = UKU_MAX(flint_px(220), ui_scrollbar_content_width(content_w, 1));

    draw_top_bar(app, font, text->create_title, 1, UKU_FOCUS_CREATE_BACK, &back_clicked, 0, NULL, view_w);
    if(back_clicked) {
        app->screen = UKU_SCREEN_HOME;
        app->active_field = UKU_FIELD_NONE;
        ui_focus_clear();
    }

    BeginScissorMode(0, viewport_y, view_w, viewport_h);
    y = draw_text_field(app, font, text->topic_question_label, text->topic_question_placeholder,
                        d->topic, sizeof(d->topic), UKU_FIELD_TOPIC, UKU_FOCUS_TOPIC,
                        content_x, y, content_w, flint_px(46));
    if(d->topic_error) {
        draw_text_font(font, text->topic_error, content_x, y - flint_px(8), small_font, C_RED);
        y += small_font + flint_px(8);
    }
    y = draw_text_field(app, font, text->description_label, text->description_placeholder,
                        d->description, sizeof(d->description), UKU_FIELD_DESCRIPTION, UKU_FOCUS_DESCRIPTION,
                        content_x, y, content_w, flint_px(82));
    y = draw_negative_weight_dropdown(app, font, text, content_x, y, UKU_MIN(content_w, flint_px(310)),
                                      UKU_FOCUS_NEGATIVE_WEIGHT);
    y += flint_px(6);
    y = draw_duration_group(app, font, text->proposal_time_label, text,
                            &d->proposal_days, &d->proposal_hours, &d->proposal_minutes,
                            content_x, y, content_w, UKU_FOCUS_PROPOSAL_DAYS_MINUS);
    y = draw_duration_group(app, font, text->voting_time_label, text,
                            &d->voting_days, &d->voting_hours, &d->voting_minutes,
                            content_x, y, content_w, UKU_FOCUS_VOTING_DAYS_MINUS);

    draw_button(app, font, content_x, y, content_w, flint_px(48), text->create_process_button, 1,
                UKU_FOCUS_CREATE_SUBMIT, &clicked);
    if(clicked) {
        d->topic_error = !has_non_space(d->topic);
        d->db_error = 0;
        if(!d->topic_error) {
            d->submitted = db_save_process(app, text);
            d->db_error = !d->submitted;
            if(d->submitted) {
                app->screen = UKU_SCREEN_COLLECT;
                app->active_field = UKU_FIELD_NONE;
                ui_focus_clear();
            }
        }
    }
    y += flint_px(62);
    if(d->submitted)
        draw_centered_text(font, text->setup_ready, content_x + content_w / 2, y, small_font, C_GREEN);
    if(d->db_error)
        draw_centered_text(font, text->db_error, content_x + content_w / 2, y, small_font, C_RED);
    EndScissorMode();

    content_bottom = y + app->create_scroll + flint_px(24);
    content_h = content_bottom - viewport_y;
    max_scroll = UKU_MAX(0, content_h - viewport_h);
    app->create_max_scroll = max_scroll;
    app->create_scrollbar_visible = max_scroll > 0;
    app->create_scroll = clampi(app->create_scroll, 0, max_scroll);
    draw_scrollbar(app, view_w - side - flint_px(8), viewport_y + flint_px(8),
                   viewport_h - flint_px(16), content_h, max_scroll,
                   &app->create_scroll, &app->create_scroll_dragging, &app->create_scroll_drag_offset);
    (void)start_y;
}

static void
draw_collect(UkuApp *app, const UkuText *text, int view_w, int view_h)
{
    int side = flint_page_side_padding();
    int content_x;
    int content_w;
    int body_font = flint_clamp_px(16, 16, 20);
    int small_font = flint_clamp_px(13, 13, 16);
    int line_h = body_font + flint_px(8);
    int y = flint_px(78);
    int back_clicked = 0;
    Font font = app->font;
    UkuDecision *d = &app->decision;

    flint_centered_column(680, side, &content_x, &content_w);

    draw_top_bar(app, font, text->collect_title, 1, UKU_FOCUS_COLLECT_BACK, &back_clicked, 0, NULL, view_w);
    if(back_clicked) {
        app->screen = UKU_SCREEN_HOME;
        db_load_processes(app);
        ui_focus_clear();
    }

    draw_text_font(font, d->topic, content_x, y, body_font, C_TEXT);
    y += body_font + flint_px(18);

    y = draw_wrapped_text(font, text->collect_intro, content_x, y, content_w, body_font, line_h, C_MUTED);
    y += flint_px(20);

    draw_text_font(font, text->local_address_label, content_x, y, small_font, C_GREEN);
    y += small_font + flint_px(8);
    DrawRectangleRounded((Rectangle){(float)content_x, (float)y, (float)content_w, (float)flint_px(46)}, 0.08f, 10, WHITE);
    DrawRectangleRoundedLinesEx((Rectangle){(float)content_x, (float)y, (float)content_w, (float)flint_px(46)}, 0.08f, 10,
                                flint_px(1), C_LINE);
    draw_text_font(font, d->local_address, content_x + flint_px(12), y + flint_px(13), body_font, C_TEXT);
    y += flint_px(70);

    draw_text_font(font, text->default_proposals_label, content_x, y, small_font, C_GREEN);
    y += small_font + flint_px(12);

    DrawRectangleRounded((Rectangle){(float)content_x, (float)y, (float)content_w, (float)flint_px(72)}, 0.08f, 10,
                         (Color){255, 255, 255, 255});
    DrawRectangleRoundedLinesEx((Rectangle){(float)content_x, (float)y, (float)content_w, (float)flint_px(72)}, 0.08f, 10,
                                flint_px(1), C_LINE);
    draw_text_font(font, text->status_quo_title, content_x + flint_px(12), y + flint_px(10), body_font, C_TEXT);
    draw_text_font(font, text->status_quo_description, content_x + flint_px(12), y + flint_px(38), small_font, C_MUTED);
    y += flint_px(84);

    DrawRectangleRounded((Rectangle){(float)content_x, (float)y, (float)content_w, (float)flint_px(72)}, 0.08f, 10,
                         (Color){255, 255, 255, 255});
    DrawRectangleRoundedLinesEx((Rectangle){(float)content_x, (float)y, (float)content_w, (float)flint_px(72)}, 0.08f, 10,
                                flint_px(1), C_LINE);
    draw_text_font(font, text->repeat_process_title, content_x + flint_px(12), y + flint_px(10), body_font, C_TEXT);
    draw_text_font(font, text->repeat_process_description, content_x + flint_px(12), y + flint_px(38), small_font, C_MUTED);

    (void)view_w;
    (void)view_h;
}

static void
draw_manual(UkuApp *app, const UkuText *text, int view_w, int view_h)
{
    int side = flint_page_side_padding();
    int content_x;
    int content_w;
    int top_h = flint_px(58);
    int viewport_y = top_h;
    int viewport_h = view_h - viewport_y;
    int title_font = flint_clamp_px(20, 20, 28);
    int body_font = flint_clamp_px(15, 15, 19);
    int line_h = body_font + flint_px(7);
    int y = viewport_y + flint_px(20) - app->manual_scroll;
    int back_clicked = 0;
    int content_bottom;
    int content_h;
    Font font = app->font;

    app->manual_scroll = clampi(app->manual_scroll - (int)(GetMouseWheelMove() * flint_px(44)),
                                0, app->manual_max_scroll);
    flint_centered_column(760, side, &content_x, &content_w);

    draw_top_bar(app, font, text->manual_title, 1, UKU_FOCUS_MANUAL_BACK, &back_clicked, 0, NULL, view_w);
    if(back_clicked) {
        app->screen = UKU_SCREEN_HOME;
        ui_focus_clear();
    }

    BeginScissorMode(0, viewport_y, view_w, viewport_h);
    draw_text_font(font, text->manual_title, content_x, y, title_font, C_TEXT);
    y += title_font + flint_px(18);
    y = draw_wrapped_text(font, text->manual_body, content_x, y, content_w, body_font, line_h, C_TEXT);
    EndScissorMode();

    content_bottom = y + app->manual_scroll + flint_px(24);
    content_h = content_bottom - viewport_y;
    app->manual_max_scroll = UKU_MAX(0, content_h - viewport_h);
    app->manual_scroll = clampi(app->manual_scroll, 0, app->manual_max_scroll);
    draw_scrollbar(app, view_w - side - flint_px(8), viewport_y + flint_px(8),
                   viewport_h - flint_px(16), content_h, app->manual_max_scroll,
                   &app->manual_scroll, &app->manual_drag_scrollbar, &app->manual_scroll_drag_offset);
}

int
main(void)
{
    UkuApp app = {0};
    UkuText text = {0};

    load_text_file(&text, LOCALE_TEXT_PATH);
    init_decision(&app, &text);
    db_init(&app);

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(520, 760, text.app_title);
    SetTargetFPS(60);
    flint_dpi_init();
    app_load_font(&app);

    while(!WindowShouldClose()) {
        int view_w = GetScreenWidth();
        int view_h = GetScreenHeight();

        flint_dpi_update(view_w, view_h);
        flint_set_dpi_scale(flint_dpi_state.ui_scale_clamped);
        flint_set_view_size(view_w, view_h);
        ui_init(view_w, view_h, flint_get_dpi_scale());
        ui_set_frame((Camera2D){0});
        ui_set_cursor_clickable(&app.cursor_clickable);
        ui_set_colors(C_TEXT, C_BG, C_GREEN, C_BLUE, flint_lighten(C_BLUE, 18), C_TEXT);

        app.cursor_clickable = 0;

        BeginDrawing();
        ClearBackground(C_BG);
        ui_focus_begin();
        ui_focus_set_text_input_active(app.active_field != UKU_FIELD_NONE);
        if(app.screen == UKU_SCREEN_HOME)
            draw_home(&app, &text, view_w, view_h);
        else if(app.screen == UKU_SCREEN_CREATE)
            draw_create_placeholder(&app, &text, view_w, view_h);
        else if(app.screen == UKU_SCREEN_COLLECT)
            draw_collect(&app, &text, view_w, view_h);
        else
            draw_manual(&app, &text, view_w, view_h);
        ui_focus_end();
        EndDrawing();

        SetMouseCursor(app.cursor_clickable ? MOUSE_CURSOR_POINTING_HAND : MOUSE_CURSOR_DEFAULT);
    }

    app_unload_font(&app);
    if(app.db != NULL)
        sqlite3_close(app.db);
    CloseWindow();
    return 0;
}
