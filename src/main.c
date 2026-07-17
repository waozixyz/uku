#include "raylib.h"
#include "flint.h"
#include "embedded_assets.h"
#include "file_dialog.h"

#if defined(PLATFORM_WEB)
typedef long long sqlite3_int64;
typedef struct sqlite3 sqlite3;
#else
#include "sqlite3.h"
#endif

#if !defined(PLATFORM_WEB)
#include <curl/curl.h>
#else
#include <emscripten.h>
#endif

#include <math.h>
#include <stddef.h>
#include <stdint.h>
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
    UKU_SCREEN_MANUAL,
    UKU_SCREEN_ACCOUNT
} UkuScreen;

typedef enum UkuField {
    UKU_FIELD_NONE,
    UKU_FIELD_TOPIC,
    UKU_FIELD_DESCRIPTION,
    UKU_FIELD_SERVER_URL,
    UKU_FIELD_ALIAS,
    UKU_FIELD_PROPOSAL_TITLE,
    UKU_FIELD_PROPOSAL_DESCRIPTION,
    UKU_FIELD_VOTE_REASON,
    UKU_FIELD_JOIN_PROCESS
} UkuField;

typedef enum UkuProcessPhase {
    UKU_PROCESS_PROPOSAL,
    UKU_PROCESS_VOTING,
    UKU_PROCESS_RESULTS
} UkuProcessPhase;

typedef struct UkuDecision {
    char id[40];
    char local_address[96];
    char owner_user_id[65];
    char visibility[16];
    char decision_type[32];
    char outcome[120];
    char review_at[64];
    char topic[180];
    char description[420];
    int proposal_days;
    int proposal_hours;
    int proposal_minutes;
    int voting_days;
    int voting_hours;
    int voting_minutes;
    int negative_weight;
    int quorum_percent;
    int require_vote_reason;
    int submitted;
    int topic_error;
    int db_error;
    int remote_error;
    sqlite3_int64 created_at;
} UkuDecision;

#define UKU_MAX_PROCESSES 64
#define UKU_MAX_PROPOSALS 32
#define UKU_MAX_VOTES 256

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

typedef struct UkuProposal {
    char id[40];
    char author_user_id[65];
    char title[180];
    char description[420];
    int score;
    int total;
    int positive_total;
    int negative_total;
    int vote_count;
} UkuProposal;

typedef struct UkuVoteInfo {
    char voter_user_id[65];
    char display_name[80];
    char reason[420];
    char updated_at[64];
} UkuVoteInfo;

typedef struct UkuAccount {
    char public_id[65];
    char public_key_hex[2625];
    char private_key_hex[5121];
    char auth_token[768];
    int loaded;
    int import_failed;
} UkuAccount;

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
    int collect_scroll;
    int collect_max_scroll;
    int collect_drag_scrollbar;
    int collect_scroll_drag_offset;
    int negative_dropdown_open;
    int process_count;
    int proposal_count;
    int vote_count;
    int current_user_voted;
    int remote_processes_loaded;
    int process_detail_loaded;
    int process_detail_loading_failed;
    int proposal_submit_failed;
    int proposal_submit_ok;
    int vote_submit_failed;
    int vote_submit_ok;
    int process_update_failed;
    int process_export_failed;
    int join_process_failed;
    int server_url_error;
    int account_alias_lookup_attempted;
    int account_public_id_modal_open;
    int account_alias_modal_open;
    int process_type_modal_open;
    char account_status[160];
    char process_status[180];
    char server_url[256];
    char alias_input[40];
    char join_process_input[160];
    char proposal_title[180];
    char proposal_description[420];
    char vote_reason[420];
    FileDialog account_import_dialog;
    FileDialog account_export_dialog;
    Font font;
    Texture2D font_shapes_texture;
    Texture2D icons[UI_ICON_TYPE_COUNT];
    int icons_loaded;
    int locale_font_ready;
    sqlite3 *db;
    UkuDecision decision;
    UkuAccount account;
    UkuProcessRow processes[UKU_MAX_PROCESSES];
    UkuProposal proposals[UKU_MAX_PROPOSALS];
    UkuVoteInfo votes[UKU_MAX_VOTES];
} UkuApp;

typedef struct UkuText {
    char app_title[64];
    char home_title[64];
    char home_subtitle[96];
    char home_summary[512];
    char start_process_button[64];
    char join_process_label[96];
    char join_process_placeholder[128];
    char join_process_button[64];
    char join_process_error[128];
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
    char proposal_phase_label[96];
    char voting_phase_label[96];
    char results_phase_label[96];
    char time_remaining_label[96];
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
    UKU_FOCUS_SETTINGS,
    UKU_FOCUS_DASHBOARD_NEW,
    UKU_FOCUS_DASHBOARD_CLOSE,
    UKU_FOCUS_MANUAL_BACK,
    UKU_FOCUS_ACCOUNT_ID,
    UKU_FOCUS_PUBLIC_ID_COPY,
    UKU_FOCUS_PUBLIC_ID_CLOSE,
    UKU_FOCUS_PUBLIC_ID_ALIAS,
    UKU_FOCUS_ALIAS_FIELD,
    UKU_FOCUS_ALIAS_CLOSE,
    UKU_FOCUS_ALIAS_SAVE,
    UKU_FOCUS_PROPOSAL_TITLE,
    UKU_FOCUS_PROPOSAL_DESCRIPTION,
    UKU_FOCUS_PROPOSAL_SUBMIT,
    UKU_FOCUS_VOTE_REASON,
    UKU_FOCUS_VOTE_SUBMIT,
    UKU_FOCUS_PROCESS_PUBLIC,
    UKU_FOCUS_PROCESS_PRIVATE,
    UKU_FOCUS_PROCESS_DELETE,
    UKU_FOCUS_PROCESS_EXPORT,
    UKU_FOCUS_JOIN_PROCESS,
    UKU_FOCUS_JOIN_PROCESS_OPEN,
    UKU_FOCUS_SCORE_BASE = 1000,
    UKU_FOCUS_PROPOSAL_DELETE_BASE = 3000,
    UKU_FOCUS_DASHBOARD_PROCESS_BASE = 100,
    UKU_FOCUS_PROCESS_TYPE_BASE = 4200,
    UKU_FOCUS_PROCESS_TYPE_CLOSE = 4290
} UkuFocusId;

typedef struct UkuProcessTemplate {
    const char *type;
    const char *title;
    const char *description;
    int proposal_days;
    int proposal_hours;
    int proposal_minutes;
    int voting_days;
    int voting_hours;
    int voting_minutes;
    int negative_weight;
    int quorum_percent;
    int require_vote_reason;
} UkuProcessTemplate;

static const UkuProcessTemplate process_templates[] = {
    {
        "consent", "Consent decision",
        "Find an option without serious unresolved resistance.",
        2, 0, 0,
        1, 0, 0,
        3, 0, 1
    },
    {
        "majority", "Majority vote",
        "Choose the option with the strongest overall support.",
        1, 0, 0,
        1, 0, 0,
        1, 0, 0
    },
    {
        "advice", "Advice process",
        "Gather input before one owner makes the call.",
        3, 0, 0,
        0, 12, 0,
        2, 0, 1
    },
    {
        "dot_vote", "Dot vote",
        "Prioritize many options quickly with lightweight scoring.",
        1, 0, 0,
        0, 12, 0,
        0, 0, 0
    }
};

#define LOCALE_FONT_NAME "ui"
#define LOCALE_FONT_TTF "assets/fonts/ui.ttf"
#define LOCALE_TEXT_PATH "locales/en.txt"
#define LOCALE_FONT_BASE_SIZE 32
#define UKU_PACKAGE_ID "xyz.waozi.uku"
#define UKU_SYNC_SERVER_URL_DEFAULT "https://api.waozi.xyz"
#define UKU_SYNC_SERVER_URL_KEY "sync_server_url"
#define UKU_SYNC_ACCOUNT_ALIAS_KEY "sync_account_alias"
#define UKU_ACCOUNT_KEY_FILE "account.key"
#define UKU_ACCOUNT_KEY_FILTER ".key"

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
    else if(strcmp(key, "join_process_label") == 0)
        copy_text(text->join_process_label, sizeof(text->join_process_label), value, len);
    else if(strcmp(key, "join_process_placeholder") == 0)
        copy_text(text->join_process_placeholder, sizeof(text->join_process_placeholder), value, len);
    else if(strcmp(key, "join_process_button") == 0)
        copy_text(text->join_process_button, sizeof(text->join_process_button), value, len);
    else if(strcmp(key, "join_process_error") == 0)
        copy_text(text->join_process_error, sizeof(text->join_process_error), value, len);
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
    else if(strcmp(key, "proposal_phase_label") == 0)
        copy_text(text->proposal_phase_label, sizeof(text->proposal_phase_label), value, len);
    else if(strcmp(key, "voting_phase_label") == 0)
        copy_text(text->voting_phase_label, sizeof(text->voting_phase_label), value, len);
    else if(strcmp(key, "results_phase_label") == 0)
        copy_text(text->results_phase_label, sizeof(text->results_phase_label), value, len);
    else if(strcmp(key, "time_remaining_label") == 0)
        copy_text(text->time_remaining_label, sizeof(text->time_remaining_label), value, len);
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
    int embedded = 0;
    char *data = LoadEmbeddedAssetText(path);
    char key[96] = {0};
    char *line;
    char *value_start = NULL;

    set_default_text(text);
    if(data != NULL)
        embedded = 1;
    else
        data = LoadFileText(path);
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

    if(embedded)
        free(data);
    else
        UnloadFileText(data);
}

static void
app_load_font(UkuApp *app)
{
    Image white;

    app->font = LoadUIFontAsset(LOCALE_FONT_TTF, LOCALE_FONT_BASE_SIZE);
    if(app->font.texture.id == 0) {
        app->font = GetFontDefault();
        app->locale_font_ready = 0;
        return;
    }
    if(!RegisterUIFont(LOCALE_FONT_NAME, app->font) ||
       !RegisterUISmallFont(LOCALE_FONT_NAME, app->font) ||
       !UseUIFont(LOCALE_FONT_NAME)) {
        UnloadUIFont(&app->font);
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
    ClearUIFonts();
    if(app->locale_font_ready)
        UnloadUIFont(&app->font);
    if(app->font_shapes_texture.id != 0)
        UnloadTexture(app->font_shapes_texture);
}

static int
measure_text_font(Font font, const char *text, int font_size)
{
    return (int)(MeasureTextEx(font, text, (float)font_size, 0).x + 0.5f);
}

static void
compact_public_id(const char *public_id, char *out, size_t out_size)
{
    size_t len;

    if(out == NULL || out_size == 0)
        return;
    out[0] = '\0';
    if(public_id == NULL)
        return;
    len = strlen(public_id);
    if(len <= 12) {
        snprintf(out, out_size, "%s", public_id);
        return;
    }
    snprintf(out, out_size, "%.*s...%.*s", 4, public_id, 4, public_id + len - 4);
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

#if !defined(PLATFORM_WEB)
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

    if(!exec_sql(app->db,
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
                 ");"
                 "create table if not exists account ("
                 "id integer primary key check(id = 1),"
                 "public_id text not null,"
                 "public_key text not null,"
                 "private_key text not null"
                 ");"
                 "create table if not exists settings ("
                 "key text primary key,"
                 "value text not null"
                 ");"))
        return 0;
    sqlite3_exec(app->db, "alter table account add column auth_token text not null default ''", NULL, NULL, NULL);
    sqlite3_exec(app->db, "alter table account add column server_url text not null default 'https://api.waozi.xyz'", NULL, NULL, NULL);
    return 1;
}
#else
static int
db_init(UkuApp *app)
{
    app->db = (sqlite3 *)app;
    return 1;
}
#endif

static int
duration_minutes(int days, int hours, int minutes)
{
    return days * 24 * 60 + hours * 60 + minutes;
}

static void
setting_load_text(UkuApp *app, const char *key, const char *fallback, char *out, size_t out_size)
{
#if defined(PLATFORM_WEB)
    (void)app;
    char *value;
    if(out == NULL || out_size == 0)
        return;
    snprintf(out, out_size, "%s", fallback != NULL ? fallback : "");
    if(key == NULL)
        return;
    value = (char *)EM_ASM_PTR({
        const key = "uku:" + UTF8ToString($0);
        const value = localStorage.getItem(key);
        if(value === null)
            return 0;
        const bytes = lengthBytesUTF8(value) + 1;
        const ptr = _malloc(bytes);
        stringToUTF8(value, ptr, bytes);
        return ptr;
    }, key);
    if(value != NULL) {
        copy_text(out, out_size, value, strlen(value));
        free(value);
    }
#else
    sqlite3_stmt *stmt = NULL;

    if(out == NULL || out_size == 0)
        return;
    snprintf(out, out_size, "%s", fallback != NULL ? fallback : "");
    if(app == NULL || app->db == NULL || key == NULL)
        return;
    if(sqlite3_prepare_v2(app->db, "select value from settings where key=?", -1, &stmt, NULL) != SQLITE_OK)
        return;
    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_TRANSIENT);
    if(sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *value = sqlite3_column_text(stmt, 0);
        copy_text(out, out_size, (const char *)(value != NULL ? value : (const unsigned char *)""),
                  strlen((const char *)(value != NULL ? value : (const unsigned char *)"")));
    }
    sqlite3_finalize(stmt);
#endif
}

static int
setting_save_text(UkuApp *app, const char *key, const char *value)
{
#if defined(PLATFORM_WEB)
    (void)app;
    if(key == NULL || value == NULL)
        return 0;
    EM_ASM({
        localStorage.setItem("uku:" + UTF8ToString($0), UTF8ToString($1));
    }, key, value);
    return 1;
#else
    sqlite3_stmt *stmt = NULL;
    int ok;

    if(app == NULL || app->db == NULL || key == NULL || value == NULL)
        return 0;
    if(sqlite3_prepare_v2(app->db, "insert or replace into settings(key, value) values(?, ?)",
                          -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, value, -1, SQLITE_TRANSIENT);
    ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
#endif
}

static int
has_prefix(const char *text, const char *prefix)
{
    return text != NULL && prefix != NULL &&
           strncmp(text, prefix, strlen(prefix)) == 0;
}

static int
url_host_boundary(char ch)
{
    return ch == '\0' || ch == ':' || ch == '/' || ch == '?' || ch == '#';
}

static int
loopback_authority_valid(const char *authority)
{
    static const char *const hosts[] = {"localhost", "127.0.0.1", "10.0.2.2"};

    if(authority == NULL || authority[0] == '\0')
        return 0;
    for(size_t i = 0; i < sizeof(hosts) / sizeof(hosts[0]); i++) {
        size_t len = strlen(hosts[i]);
        if(strncmp(authority, hosts[i], len) == 0 && url_host_boundary(authority[len]))
            return 1;
    }
    return 0;
}

static int
sync_url_valid(const char *url)
{
    if(url == NULL || url[0] == '\0')
        return 0;
    if(has_prefix(url, "https://"))
        return url[8] != '\0';
    if(has_prefix(url, "http://"))
        return loopback_authority_valid(url + 7);
    return loopback_authority_valid(url);
}

static int
sync_url_normalize(const char *input, char *out, size_t out_size)
{
    int len;

    if(out == NULL || out_size == 0)
        return 0;
    out[0] = '\0';
    if(!sync_url_valid(input))
        return 0;
    if(has_prefix(input, "https://") || has_prefix(input, "http://"))
        len = snprintf(out, out_size, "%s", input);
    else
        len = snprintf(out, out_size, "http://%s", input);
    return len > 0 && (size_t)len < out_size;
}

static int
sync_server_save(UkuApp *app)
{
    char normalized[sizeof(app->server_url)];

    if(!sync_url_normalize(app->server_url, normalized, sizeof(normalized))) {
        app->server_url_error = 1;
        return 0;
    }
    snprintf(app->server_url, sizeof(app->server_url), "%s", normalized);
    app->server_url_error = 0;
    return setting_save_text(app, UKU_SYNC_SERVER_URL_KEY, normalized);
}

static void
account_to_lyra(const UkuAccount *account, LyraAccount *out)
{
    if(out == NULL)
        return;
    memset(out, 0, sizeof(*out));
    if(account == NULL)
        return;
    snprintf(out->public_id, sizeof(out->public_id), "%s", account->public_id);
    snprintf(out->public_key_hex, sizeof(out->public_key_hex), "%s", account->public_key_hex);
    snprintf(out->private_key_hex, sizeof(out->private_key_hex), "%s", account->private_key_hex);
}

static void
account_from_lyra(UkuAccount *account, const LyraAccount *source)
{
    char auth_token[sizeof(account->auth_token)];
    int import_failed;

    if(account == NULL || source == NULL)
        return;
    snprintf(auth_token, sizeof(auth_token), "%s", account->auth_token);
    import_failed = account->import_failed;
    memset(account, 0, sizeof(*account));
    snprintf(account->public_id, sizeof(account->public_id), "%s", source->public_id);
    snprintf(account->public_key_hex, sizeof(account->public_key_hex), "%s", source->public_key_hex);
    snprintf(account->private_key_hex, sizeof(account->private_key_hex), "%s", source->private_key_hex);
    snprintf(account->auth_token, sizeof(account->auth_token), "%s", auth_token);
    account->import_failed = import_failed;
    account->loaded = 1;
}

static void
alias_normalize(char *text)
{
    size_t write = 0;

    if(text == NULL)
        return;
    for(size_t read = 0; text[read] != '\0'; read++) {
        char c = text[read];
        if(c >= 'A' && c <= 'Z')
            c = (char)(c - 'A' + 'a');
        if(c == '@' && write == 0)
            continue;
        if((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_') {
            if(write + 1 < 40)
                text[write++] = c;
        }
    }
    text[write] = '\0';
}

static int
alias_valid(const char *text)
{
    size_t len;

    if(text == NULL)
        return 0;
    len = strlen(text);
    if(len < 4 || len > 32)
        return 0;
    for(size_t i = 0; i < len; i++) {
        char c = text[i];
        if((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')
            continue;
        return 0;
    }
    return 1;
}

static int
account_validate_import(UkuAccount *account)
{
    LyraAccount lyra_account;

    if(account == NULL)
        return 0;
    account_to_lyra(account, &lyra_account);
    if(!ValidateLyraAccount(&lyra_account))
        return 0;
    account_from_lyra(account, &lyra_account);
    return 1;
}

static int
account_parse_key_text(const char *body, UkuAccount *account)
{
    LyraAccount parsed;

    if(body == NULL || account == NULL)
        return 0;
    if(!ParseLyraAccountText(body, &parsed))
        return 0;
    memset(account, 0, sizeof(*account));
    account_from_lyra(account, &parsed);
    return 1;
}

static int
account_has_values(const UkuAccount *account)
{
    LyraAccount lyra_account;

    account_to_lyra(account, &lyra_account);
    return ValidateLyraAccount(&lyra_account);
}

static int
account_save(UkuApp *app, const UkuAccount *account)
{
#if defined(PLATFORM_WEB)
    if(app == NULL || !account_has_values(account))
        return 0;
    app->account = *account;
    return setting_save_text(app, "account_public_id", account->public_id) &&
           setting_save_text(app, "account_public_key", account->public_key_hex) &&
           setting_save_text(app, "account_private_key", account->private_key_hex) &&
           setting_save_text(app, "account_auth_token", account->auth_token);
#else
    sqlite3_stmt *stmt = NULL;
    int ok;

    if(app->db == NULL || !account_has_values(account))
        return 0;
    if(sqlite3_prepare_v2(app->db,
                          "insert or replace into account(id, public_id, public_key, private_key, auth_token) values(1, ?, ?, ?, ?)",
                          -1, &stmt, NULL) != SQLITE_OK)
        return 0;
    sqlite3_bind_text(stmt, 1, account->public_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, account->public_key_hex, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, account->private_key_hex, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, account->auth_token, -1, SQLITE_TRANSIENT);
    ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
#endif
}

static void
account_load(UkuApp *app)
{
#if defined(PLATFORM_WEB)
    memset(&app->account, 0, sizeof(app->account));
    setting_load_text(app, "account_public_id", "", app->account.public_id, sizeof(app->account.public_id));
    setting_load_text(app, "account_public_key", "", app->account.public_key_hex, sizeof(app->account.public_key_hex));
    setting_load_text(app, "account_private_key", "", app->account.private_key_hex, sizeof(app->account.private_key_hex));
    setting_load_text(app, "account_auth_token", "", app->account.auth_token, sizeof(app->account.auth_token));
    app->account.loaded = account_has_values(&app->account);
#else
    sqlite3_stmt *stmt = NULL;

    memset(&app->account, 0, sizeof(app->account));
    if(app->db == NULL)
        return;
    if(sqlite3_prepare_v2(app->db,
                          "select public_id, public_key, private_key, auth_token from account where id=1",
                          -1, &stmt, NULL) != SQLITE_OK)
        return;
    if(sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *public_id = sqlite3_column_text(stmt, 0);
        const unsigned char *public_key = sqlite3_column_text(stmt, 1);
        const unsigned char *private_key = sqlite3_column_text(stmt, 2);
        const unsigned char *auth_token = sqlite3_column_text(stmt, 3);
        copy_text(app->account.public_id, sizeof(app->account.public_id),
                  (const char *)(public_id != NULL ? public_id : (const unsigned char *)""),
                  strlen((const char *)(public_id != NULL ? public_id : (const unsigned char *)"")));
        copy_text(app->account.public_key_hex, sizeof(app->account.public_key_hex),
                  (const char *)(public_key != NULL ? public_key : (const unsigned char *)""),
                  strlen((const char *)(public_key != NULL ? public_key : (const unsigned char *)"")));
        copy_text(app->account.private_key_hex, sizeof(app->account.private_key_hex),
                  (const char *)(private_key != NULL ? private_key : (const unsigned char *)""),
                  strlen((const char *)(private_key != NULL ? private_key : (const unsigned char *)"")));
        copy_text(app->account.auth_token, sizeof(app->account.auth_token),
                  (const char *)(auth_token != NULL ? auth_token : (const unsigned char *)""),
                  strlen((const char *)(auth_token != NULL ? auth_token : (const unsigned char *)"")));
        app->account.loaded = account_has_values(&app->account);
    }
    sqlite3_finalize(stmt);
#endif
}

static int
account_import_file(UkuApp *app, const char *path)
{
    char *body;
    UkuAccount imported;

    app->account.import_failed = 0;
    app->account_status[0] = '\0';
    body = LoadFileText(path);
    if(body == NULL) {
        app->account.import_failed = 1;
        copy_text(app->account_status, sizeof(app->account_status),
                  "Import failed. Choose a readable account key file.",
                  strlen("Import failed. Choose a readable account key file."));
        return 0;
    }
    if(!account_parse_key_text(body, &imported) || !account_validate_import(&imported) ||
       !account_save(app, &imported)) {
        app->account.import_failed = 1;
        copy_text(app->account_status, sizeof(app->account_status),
                  "Import failed. The selected key is not valid.",
                  strlen("Import failed. The selected key is not valid."));
        UnloadFileText(body);
        return 0;
    }
    UnloadFileText(body);
    app->account = imported;
    app->account.loaded = 1;
    app->account_alias_lookup_attempted = 0;
    copy_text(app->account_status, sizeof(app->account_status),
              "Account key imported.", strlen("Account key imported."));
    return 1;
}

static int
account_export_file(UkuApp *app, const char *path)
{
    char body[LYRA_ACCOUNT_EXPORT_TEXT_SIZE];
    LyraAccount lyra_account;
    int ok;

    if(app == NULL || !account_has_values(&app->account) || path == NULL || path[0] == '\0')
        return 0;
    account_to_lyra(&app->account, &lyra_account);
    if(!ExportLyraAccountText(&lyra_account, body, sizeof(body)))
        return 0;
    ok = SaveFileData(path, body, (int)strlen(body));
    copy_text(app->account_status, sizeof(app->account_status),
              ok ? "Account key exported." : "Account key export failed.",
              strlen(ok ? "Account key exported." : "Account key export failed."));
    return ok;
}

static void
account_apply_file_dialog_theme(void)
{
#if !defined(PLATFORM_WEB)
    SetFileDialogThemeScope(GetThemeScopeName(THEME_SUNSET, GetThemeDarkMode()));
#endif
}

static void
account_start_import_dialog(UkuApp *app)
{
#if defined(PLATFORM_WEB)
    if(app == NULL)
        return;
    app->account.import_failed = 1;
    copy_text(app->account_status, sizeof(app->account_status),
              "Account key import is desktop-only in this web build.",
              strlen("Account key import is desktop-only in this web build."));
#else
    const char *path;

    if(app == NULL)
        return;
    account_apply_file_dialog_theme();
    app->account.import_failed = 0;
    app->account_status[0] = '\0';
    if(LoadFilteredFileDialog(&app->account_import_dialog, "Import account key",
                              UKU_ACCOUNT_KEY_FILTER)) {
        path = GetFileDialogPath(&app->account_import_dialog);
        if(path != NULL && path[0] != '\0') {
            account_import_file(app, path);
            return;
        }
    }
    copy_text(app->account_status, sizeof(app->account_status),
              "Import cancelled.", strlen("Import cancelled."));
#endif
}

static void
account_start_export_dialog(UkuApp *app)
{
#if defined(PLATFORM_WEB)
    if(app == NULL)
        return;
    copy_text(app->account_status, sizeof(app->account_status),
              "Account key export is desktop-only in this web build.",
              strlen("Account key export is desktop-only in this web build."));
#else
    const char *path;

    if(app == NULL || !account_has_values(&app->account))
        return;
    account_apply_file_dialog_theme();
    app->account_status[0] = '\0';
    if(SaveFileDialog(&app->account_export_dialog, "Export account key",
                      UKU_ACCOUNT_KEY_FILE)) {
        path = GetFileDialogPath(&app->account_export_dialog);
        if(path != NULL && path[0] != '\0') {
            account_export_file(app, path);
            return;
        }
    }
    copy_text(app->account_status, sizeof(app->account_status),
              "Export cancelled.", strlen("Export cancelled."));
#endif
}

static int
account_create(UkuApp *app)
{
    LyraAccount lyra_account;
    UkuAccount generated;

    memset(&generated, 0, sizeof(generated));
    if(!CreateLyraAccount(&lyra_account)) {
        copy_text(app->account_status, sizeof(app->account_status),
                  "Account creation failed.", strlen("Account creation failed."));
        return 0;
    }
    account_from_lyra(&generated, &lyra_account);
    if(!account_save(app, &generated)) {
        copy_text(app->account_status, sizeof(app->account_status),
                  "Account creation failed.", strlen("Account creation failed."));
        return 0;
    }
    app->account = generated;
    app->account_alias_lookup_attempted = 0;
    copy_text(app->account_status, sizeof(app->account_status),
              "Account created.", strlen("Account created."));
    return 1;
}

static int
account_sign_hex(UkuApp *app, const uint8_t *message, size_t message_len,
                 char *out_signature_hex, size_t out_size)
{
    LyraAccount lyra_account;

    if(app == NULL || !app->account.loaded)
        return 0;
    account_to_lyra(&app->account, &lyra_account);
    return SignLyraAccountHex(&lyra_account, message, message_len, out_signature_hex,
                                       out_size);
}

typedef struct UkuHttpBuffer {
    char *data;
    size_t len;
    size_t cap;
} UkuHttpBuffer;

typedef struct UkuHttpHeaders {
#if !defined(PLATFORM_WEB)
    struct curl_slist *native;
#else
    char lines[8192];
#endif
} UkuHttpHeaders;

static int
http_buffer_append(UkuHttpBuffer *buffer, const char *data, size_t len)
{
    char *next;
    size_t next_cap;

    if(buffer == NULL || data == NULL || len == 0)
        return 1;
    if(buffer->cap == 0 || len >= buffer->cap - buffer->len) {
        next_cap = buffer->cap > 0 ? buffer->cap : 4096;
        while(len >= next_cap - buffer->len)
            next_cap *= 2;
        next = (char *)realloc(buffer->data, next_cap);
        if(next == NULL)
            return 0;
        buffer->data = next;
        buffer->cap = next_cap;
    }
    memcpy(buffer->data + buffer->len, data, len);
    buffer->len += len;
    buffer->data[buffer->len] = '\0';
    return 1;
}

static int
json_append_string(UkuHttpBuffer *buffer, const char *text)
{
    if(!http_buffer_append(buffer, "\"", 1))
        return 0;
    if(text == NULL)
        text = "";
    for(const char *p = text; *p != '\0'; p++) {
        char one[2] = {*p, '\0'};
        if(*p == '"' && !http_buffer_append(buffer, "\\\"", 2))
            return 0;
        else if(*p == '\\' && !http_buffer_append(buffer, "\\\\", 2))
            return 0;
        else if(*p == '\n' && !http_buffer_append(buffer, "\\n", 2))
            return 0;
        else if(*p == '\r' && !http_buffer_append(buffer, "\\r", 2))
            return 0;
        else if(*p == '\t' && !http_buffer_append(buffer, "\\t", 2))
            return 0;
        else if(*p >= 32 && !http_buffer_append(buffer, one, 1))
            return 0;
    }
    return http_buffer_append(buffer, "\"", 1);
}

static UkuHttpHeaders *
http_headers_append(UkuHttpHeaders *headers, const char *line)
{
    if(line == NULL)
        return headers;
    if(headers == NULL) {
        headers = (UkuHttpHeaders *)calloc(1, sizeof(*headers));
        if(headers == NULL)
            return NULL;
    }
#if !defined(PLATFORM_WEB)
    headers->native = curl_slist_append(headers->native, line);
    if(headers->native == NULL) {
        free(headers);
        return NULL;
    }
#else
    if(headers->lines[0] != '\0')
        strncat(headers->lines, "\n", sizeof(headers->lines) - strlen(headers->lines) - 1);
    strncat(headers->lines, line, sizeof(headers->lines) - strlen(headers->lines) - 1);
#endif
    return headers;
}

static void
http_headers_free(UkuHttpHeaders *headers)
{
    if(headers == NULL)
        return;
#if !defined(PLATFORM_WEB)
    if(headers->native != NULL)
        curl_slist_free_all(headers->native);
#endif
    free(headers);
}

#if !defined(PLATFORM_WEB)
static size_t
curl_write_cb(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    size_t bytes = size * nmemb;
    UkuHttpBuffer *buffer = (UkuHttpBuffer *)userdata;
    return http_buffer_append(buffer, ptr, bytes) ? bytes : 0;
}
#endif

static int
extract_json_string(const char *json, const char *key, char *out, size_t out_size)
{
    char pattern[64];
    const char *p;
    size_t n = 0;

    if(json == NULL || key == NULL || out == NULL || out_size == 0)
        return 0;
    out[0] = '\0';
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    p = strstr(json, pattern);
    if(p == NULL)
        return 0;
    p = strchr(p + strlen(pattern), ':');
    if(p == NULL)
        return 0;
    p++;
    while(*p == ' ' || *p == '\t')
        p++;
    if(*p != '"')
        return 0;
    p++;
    while(*p != '\0' && *p != '"' && n + 1 < out_size) {
        if(*p == '\\' && p[1] != '\0')
            p++;
        out[n++] = *p++;
    }
    out[n] = '\0';
    return n > 0;
}

static int
extract_json_int(const char *json, const char *key, int *out)
{
    char pattern[64];
    const char *p;

    if(json == NULL || key == NULL || out == NULL)
        return 0;
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    p = strstr(json, pattern);
    if(p == NULL)
        return 0;
    p = strchr(p + strlen(pattern), ':');
    if(p == NULL)
        return 0;
    p++;
    while(*p == ' ' || *p == '\t')
        p++;
    *out = atoi(p);
    return 1;
}

static int
extract_json_bool(const char *json, const char *key, int *out)
{
    char pattern[64];
    const char *p;

    if(json == NULL || key == NULL || out == NULL)
        return 0;
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    p = strstr(json, pattern);
    if(p == NULL)
        return 0;
    p = strchr(p + strlen(pattern), ':');
    if(p == NULL)
        return 0;
    p++;
    while(*p == ' ' || *p == '\t')
        p++;
    if(strncmp(p, "true", 4) == 0) {
        *out = 1;
        return 1;
    }
    if(strncmp(p, "false", 5) == 0) {
        *out = 0;
        return 1;
    }
    *out = atoi(p) != 0;
    return 1;
}

static sqlite3_int64
parse_lyra_time(const char *text)
{
    int year, month, day, hour, minute, second;
    struct tm tmv;

    if(text == NULL)
        return (sqlite3_int64)time(NULL);
    year = month = day = hour = minute = second = 0;
    if(sscanf(text, "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second) != 6)
        return (sqlite3_int64)time(NULL);
    memset(&tmv, 0, sizeof(tmv));
    tmv.tm_year = year - 1900;
    tmv.tm_mon = month - 1;
    tmv.tm_mday = day;
    tmv.tm_hour = hour;
    tmv.tm_min = minute;
    tmv.tm_sec = second;
    return (sqlite3_int64)mktime(&tmv);
}

static void
proposals_clear(UkuApp *app)
{
    if(app == NULL)
        return;
    app->proposal_count = 0;
    app->vote_count = 0;
    app->current_user_voted = 0;
    memset(app->proposals, 0, sizeof(app->proposals));
    memset(app->votes, 0, sizeof(app->votes));
}

static int
proposal_find(UkuApp *app, const char *id)
{
    if(app == NULL || id == NULL)
        return -1;
    for(int i = 0; i < app->proposal_count; i++) {
        if(strcmp(app->proposals[i].id, id) == 0)
            return i;
    }
    return -1;
}

static int
proposal_add(UkuApp *app, const char *id, const char *title, const char *description)
{
    UkuProposal *proposal;
    int existing;

    if(app == NULL || app->proposal_count >= UKU_MAX_PROPOSALS || id == NULL || title == NULL)
        return -1;
    existing = proposal_find(app, id);
    if(existing >= 0)
        return existing;
    proposal = &app->proposals[app->proposal_count++];
    memset(proposal, 0, sizeof(*proposal));
    copy_text(proposal->id, sizeof(proposal->id), id, strlen(id));
    copy_text(proposal->title, sizeof(proposal->title), title, strlen(title));
    copy_text(proposal->description, sizeof(proposal->description),
              description != NULL ? description : "",
              strlen(description != NULL ? description : ""));
    return app->proposal_count - 1;
}

static void
proposal_reset_totals(UkuApp *app)
{
    if(app == NULL)
        return;
    for(int i = 0; i < app->proposal_count; i++) {
        app->proposals[i].total = 0;
        app->proposals[i].positive_total = 0;
        app->proposals[i].negative_total = 0;
        app->proposals[i].vote_count = 0;
    }
}

static void
load_default_proposals(UkuApp *app, const UkuText *text)
{
    if(text == NULL)
        return;
    proposals_clear(app);
    proposal_add(app, "status-quo", text->status_quo_title, text->status_quo_description);
    proposal_add(app, "repeat-process", text->repeat_process_title, text->repeat_process_description);
}

static const char *
json_array_start(const char *json, const char *key)
{
    char pattern[64];
    const char *p;

    if(json == NULL || key == NULL)
        return NULL;
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    p = strstr(json, pattern);
    if(p == NULL)
        return NULL;
    p = strchr(p + strlen(pattern), '[');
    return p != NULL ? p + 1 : NULL;
}

static const char *
json_object_end(const char *p)
{
    const char *start = p;
    int depth = 0;
    int in_string = 0;

    if(p == NULL || *p != '{')
        return NULL;
    for(; *p != '\0'; p++) {
        if(*p == '"' && (p == start || p[-1] != '\\'))
            in_string = !in_string;
        if(in_string)
            continue;
        if(*p == '{')
            depth++;
        else if(*p == '}') {
            depth--;
            if(depth == 0)
                return p;
        }
    }
    return NULL;
}

static void
parse_process_proposals(UkuApp *app, const char *json)
{
    const char *p = json_array_start(json, "proposals");

    while(p != NULL && *p != '\0' && *p != ']') {
        const char *end;
        char object[2048];
        char id[40];
        char author[65];
        char title[180];
        char description[420];
        size_t len;

        while(*p != '\0' && *p != '{' && *p != ']')
            p++;
        if(*p != '{')
            break;
        end = json_object_end(p);
        if(end == NULL)
            break;
        len = UKU_MIN((size_t)(end - p + 1), sizeof(object) - 1);
        memcpy(object, p, len);
        object[len] = '\0';
        author[0] = '\0';
        description[0] = '\0';
        if(extract_json_string(object, "id", id, sizeof(id)) &&
           extract_json_string(object, "title", title, sizeof(title))) {
            extract_json_string(object, "description", description, sizeof(description));
            extract_json_string(object, "author_user_id_hash", author, sizeof(author));
            int index = proposal_add(app, id, title, description);
            if(index >= 0 && author[0] != '\0')
                copy_text(app->proposals[index].author_user_id,
                          sizeof(app->proposals[index].author_user_id),
                          author, strlen(author));
        }
        p = end + 1;
    }
}

static void
parse_vote_scores(UkuApp *app, const char *object, int current_user_vote)
{
    const char *scores = strstr(object, "\"scores\"");
    const char *p;

    if(scores == NULL)
        return;
    p = strchr(scores, '{');
    if(p == NULL)
        return;
    p++;
    while(*p != '\0' && *p != '}') {
        char id[40];
        int score;
        size_t n = 0;
        int index;

        while(*p != '\0' && *p != '"' && *p != '}')
            p++;
        if(*p != '"')
            break;
        p++;
        while(*p != '\0' && *p != '"' && n + 1 < sizeof(id))
            id[n++] = *p++;
        id[n] = '\0';
        p = strchr(p, ':');
        if(p == NULL)
            break;
        p++;
        score = atoi(p);
        index = proposal_find(app, id);
        if(index >= 0) {
            int weighted = score < 0 ? score * app->decision.negative_weight : score;
            app->proposals[index].total += weighted;
            if(current_user_vote)
                app->proposals[index].score = score;
            if(score < 0)
                app->proposals[index].negative_total += weighted;
            else
                app->proposals[index].positive_total += score;
            app->proposals[index].vote_count++;
        }
        while(*p != '\0' && *p != ',' && *p != '}')
            p++;
        if(*p == ',')
            p++;
    }
}

static void
parse_process_votes(UkuApp *app, const char *json)
{
    const char *p = json_array_start(json, "votes");

    app->vote_count = 0;
    app->current_user_voted = 0;
    memset(app->votes, 0, sizeof(app->votes));
    while(p != NULL && *p != '\0' && *p != ']' && app->vote_count < UKU_MAX_VOTES) {
        const char *end;
        char object[4096];
        UkuVoteInfo *vote;
        size_t len;
        int current_user_vote;

        while(*p != '\0' && *p != '{' && *p != ']')
            p++;
        if(*p != '{')
            break;
        end = json_object_end(p);
        if(end == NULL)
            break;
        len = UKU_MIN((size_t)(end - p + 1), sizeof(object) - 1);
        memcpy(object, p, len);
        object[len] = '\0';
        vote = &app->votes[app->vote_count];
        extract_json_string(object, "voter_user_id_hash", vote->voter_user_id, sizeof(vote->voter_user_id));
        extract_json_string(object, "display_name", vote->display_name, sizeof(vote->display_name));
        extract_json_string(object, "reason", vote->reason, sizeof(vote->reason));
        extract_json_string(object, "updated_at", vote->updated_at, sizeof(vote->updated_at));
        current_user_vote = app->account.loaded && vote->voter_user_id[0] != '\0' &&
                            strcmp(vote->voter_user_id, app->account.public_id) == 0;
        if(current_user_vote)
            app->current_user_voted = 1;
        parse_vote_scores(app, object, current_user_vote);
        app->vote_count++;
        p = end + 1;
    }
}

static void
join_url(char *out, size_t out_size, const char *base, const char *path)
{
    size_t len;

    if(out == NULL || out_size == 0)
        return;
    out[0] = '\0';
    if(base == NULL || path == NULL)
        return;
    len = strlen(base);
    while(len > 0 && base[len - 1] == '/')
        len--;
    snprintf(out, out_size, "%.*s%s", (int)len, base, path);
}

static void
canonical_message_hex(const char *nonce_hex, const char *method, const char *path,
                      const char *body, char *out, size_t out_size)
{
    char digest_hex[65];

    LyraSha256Hex((const uint8_t *)body, strlen(body), digest_hex);
    snprintf(out, out_size, "inbe-sync-v1\n%s\n%s\n%s\n%s\n", method, path, digest_hex, nonce_hex);
}

#if !defined(PLATFORM_WEB)
static int
lyra_http_request(const char *method, const char *url, UkuHttpHeaders *headers,
                  const char *body, long *status_out, UkuHttpBuffer *response)
{
    CURL *curl;
    CURLcode code;
    long status = 0;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if(curl == NULL)
        return 0;
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 12L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    if(headers != NULL && headers->native != NULL)
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers->native);
    if(strcmp(method, "POST") == 0) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body != NULL ? body : "");
    } else if(strcmp(method, "PATCH") == 0) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body != NULL ? body : "");
    } else if(strcmp(method, "DELETE") == 0) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    }
    code = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);
    if(status_out != NULL)
        *status_out = status;
    return code == CURLE_OK;
}
#else
static int
lyra_http_request(const char *method, const char *url, UkuHttpHeaders *headers,
                  const char *body, long *status_out, UkuHttpBuffer *response)
{
    int ok;
    int status = 0;
    const char *header_lines = headers != NULL ? headers->lines : "";
    const char *request_body = body != NULL ? body : "";

    if(method == NULL || url == NULL || response == NULL)
        return 0;
    response->data = NULL;
    response->len = 0;
    response->cap = 0;
    ok = EM_ASM_INT({
        const method = UTF8ToString($0);
        const url = UTF8ToString($1);
        const headerText = UTF8ToString($2);
        const body = UTF8ToString($3);
        const xhr = new XMLHttpRequest();
        try {
            xhr.open(method, url, false);
            headerText.split("\n").forEach(function(line) {
                const index = line.indexOf(":");
                if(index > 0)
                    xhr.setRequestHeader(line.slice(0, index).trim(), line.slice(index + 1).trim());
            });
            xhr.send((method === "GET" || method === "DELETE") ? null : body);
        } catch(err) {
            console.error("Uku Lyra request failed:", err);
            setValue($4, 0, "i32");
            return 0;
        }
        const text = xhr.responseText || "";
        const bytes = lengthBytesUTF8(text) + 1;
        const ptr = _malloc(bytes);
        if(ptr === 0) {
            setValue($4, xhr.status || 0, "i32");
            return 0;
        }
        stringToUTF8(text, ptr, bytes);
        setValue($5, ptr, "*");
        setValue($6, bytes - 1, "i32");
        setValue($7, bytes, "i32");
        setValue($4, xhr.status || 0, "i32");
        return 1;
    }, method, url, header_lines, request_body, &status, &response->data, &response->len, &response->cap);
    if(status_out != NULL)
        *status_out = status;
    return ok != 0;
}
#endif

static int
lyra_login(UkuApp *app, const char *base_url)
{
    char url[512];
    char challenge_path[160];
    char nonce[80];
    char body[3100];
    char message[3600];
    char signature[4841];
    char user_header[96];
    char account_alias[40];
    UkuHttpBuffer response = {0};
    UkuHttpHeaders *headers = NULL;
    long status = 0;
    int ok = 0;

    if(app == NULL || !app->account.loaded)
        return 0;
    snprintf(challenge_path, sizeof(challenge_path), "/api/v1/sync/challenge?user_id=%s", app->account.public_id);
    join_url(url, sizeof(url), base_url, challenge_path);
    if(!lyra_http_request("GET", url, NULL, NULL, &status, &response) || status != 200 ||
       !extract_json_string(response.data, "nonce", nonce, sizeof(nonce)))
        goto cleanup;
    free(response.data);
    response = (UkuHttpBuffer){0};

    snprintf(body, sizeof(body), "{\"user_id_hash\":\"%s\",\"client_id\":\"uku-native-client\",\"public_key\":\"%s\"}",
             app->account.public_id, app->account.public_key_hex);
    canonical_message_hex(nonce, "POST", "/api/v1/sync/login", body, message, sizeof(message));
    if(!account_sign_hex(app, (const uint8_t *)message, strlen(message), signature, sizeof(signature)))
        goto cleanup;
    join_url(url, sizeof(url), base_url, "/api/v1/sync/login");
    snprintf(user_header, sizeof(user_header), "X-Inbe-User: %s", app->account.public_id);
    headers = http_headers_append(headers, "Content-Type: application/json");
    headers = http_headers_append(headers, user_header);
    {
        char sig_header[4880];
        snprintf(sig_header, sizeof(sig_header), "X-Inbe-Signature: %s", signature);
        headers = http_headers_append(headers, sig_header);
    }
    if(!lyra_http_request("POST", url, headers, body, &status, &response) || status != 200 ||
       !extract_json_string(response.data, "auth_token", app->account.auth_token, sizeof(app->account.auth_token)))
        goto cleanup;
    if(extract_json_string(response.data, "account_alias", account_alias, sizeof(account_alias)) &&
       account_alias[0] != '\0')
        setting_save_text(app, UKU_SYNC_ACCOUNT_ALIAS_KEY, account_alias);
    account_save(app, &app->account);
    ok = 1;

cleanup:
    if(headers != NULL)
        http_headers_free(headers);
    free(response.data);
    return ok;
}

static char *
build_remote_process_json(UkuApp *app)
{
    UkuHttpBuffer json = {0};
    UkuDecision *d = &app->decision;
    char tmp[512];

    if(!http_buffer_append(&json, "{\"user_id_hash\":", 16) ||
       !json_append_string(&json, app->account.public_id) ||
       !http_buffer_append(&json, ",\"id\":", 6) ||
       !json_append_string(&json, d->id) ||
       !http_buffer_append(&json, ",\"question\":", 12) ||
       !json_append_string(&json, d->topic) ||
       !http_buffer_append(&json, ",\"description\":", 15) ||
       !json_append_string(&json, d->description))
        goto fail;
    snprintf(tmp, sizeof(tmp),
             ",\"visibility\":\"public\",\"decision_type\":\"%s\",\"proposal_minutes\":%d,\"voting_minutes\":%d,\"negative_weight\":%d,\"quorum_percent\":%d,\"require_vote_reason\":%s}",
             d->decision_type[0] != '\0' ? d->decision_type : "consent",
             duration_minutes(d->proposal_days, d->proposal_hours, d->proposal_minutes),
             duration_minutes(d->voting_days, d->voting_hours, d->voting_minutes),
             d->negative_weight, d->quorum_percent, d->require_vote_reason ? "true" : "false");
    if(!http_buffer_append(&json, tmp, strlen(tmp)))
        goto fail;
    return json.data;

fail:
    free(json.data);
    return NULL;
}

static void parse_process_detail(UkuApp *app, const char *json, const UkuText *text);

static int
lyra_create_process(UkuApp *app, const char *base_url)
{
    char url[512];
    char user_header[96];
    char auth_header[900];
    char *body;
    UkuHttpBuffer response = {0};
    UkuHttpHeaders *headers = NULL;
    long status = 0;
    int ok = 0;

    if(app == NULL || !app->account.loaded)
        return 0;
    if(app->account.auth_token[0] == '\0' && !lyra_login(app, base_url))
        return 0;
    body = build_remote_process_json(app);
    if(body == NULL)
        return 0;
    join_url(url, sizeof(url), base_url, "/api/v1/governance/processes");
    snprintf(user_header, sizeof(user_header), "X-Inbe-User: %s", app->account.public_id);
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", app->account.auth_token);
    headers = http_headers_append(headers, "Content-Type: application/json");
    headers = http_headers_append(headers, user_header);
    headers = http_headers_append(headers, auth_header);
    if(lyra_http_request("POST", url, headers, body, &status, &response) && status == 201) {
        if(response.data != NULL)
            parse_process_detail(app, response.data, NULL);
        ok = 1;
    } else if(status == 401) {
        app->account.auth_token[0] = '\0';
        account_save(app, &app->account);
        if(headers != NULL)
            http_headers_free(headers);
        headers = NULL;
        free(response.data);
        response = (UkuHttpBuffer){0};
        if(lyra_login(app, base_url)) {
            snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", app->account.auth_token);
            headers = http_headers_append(headers, "Content-Type: application/json");
            headers = http_headers_append(headers, user_header);
            headers = http_headers_append(headers, auth_header);
            ok = lyra_http_request("POST", url, headers, body, &status, &response) && status == 201;
            if(ok && response.data != NULL)
                parse_process_detail(app, response.data, NULL);
        }
    }
    if(headers != NULL)
        http_headers_free(headers);
    free(response.data);
    free(body);
    return ok;
}

static int
lyra_authorized_json(UkuApp *app, const char *base_url, const char *method,
                     const char *path, const char *body, int ok_min, int ok_max,
                     UkuHttpBuffer *response)
{
    char url[512];
    char user_header[96];
    char auth_header[900];
    UkuHttpHeaders *headers = NULL;
    long status = 0;
    int ok = 0;

    if(app == NULL || !app->account.loaded || path == NULL)
        return 0;
    if(app->account.auth_token[0] == '\0' && !lyra_login(app, base_url))
        return 0;
    join_url(url, sizeof(url), base_url, path);
    snprintf(user_header, sizeof(user_header), "X-Inbe-User: %s", app->account.public_id);
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", app->account.auth_token);
    headers = http_headers_append(headers, "Content-Type: application/json");
    headers = http_headers_append(headers, user_header);
    headers = http_headers_append(headers, auth_header);
    ok = lyra_http_request(method, url, headers, body, &status, response) &&
         status >= ok_min && status <= ok_max;
    if(!ok && status == 401) {
        if(headers != NULL)
            http_headers_free(headers);
        headers = NULL;
        if(response != NULL) {
            free(response->data);
            *response = (UkuHttpBuffer){0};
        }
        app->account.auth_token[0] = '\0';
        account_save(app, &app->account);
        if(lyra_login(app, base_url)) {
            snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", app->account.auth_token);
            headers = http_headers_append(headers, "Content-Type: application/json");
            headers = http_headers_append(headers, user_header);
            headers = http_headers_append(headers, auth_header);
            ok = lyra_http_request(method, url, headers, body, &status, response) &&
                 status >= ok_min && status <= ok_max;
        }
    }
    if(headers != NULL)
        http_headers_free(headers);
    return ok;
}

static void
parse_process_detail(UkuApp *app, const char *json, const UkuText *text)
{
    char created_at[64];
    int total_minutes;

    if(app == NULL || json == NULL)
        return;
    if(extract_json_string(json, "id", app->decision.id, sizeof(app->decision.id)))
        snprintf(app->decision.local_address, sizeof(app->decision.local_address),
                 "/app/%s/collect", app->decision.id);
    extract_json_string(json, "owner_user_id_hash", app->decision.owner_user_id,
                        sizeof(app->decision.owner_user_id));
    if(!extract_json_string(json, "visibility", app->decision.visibility,
                            sizeof(app->decision.visibility)))
        copy_text(app->decision.visibility, sizeof(app->decision.visibility), "public", strlen("public"));
    extract_json_string(json, "question", app->decision.topic, sizeof(app->decision.topic));
    extract_json_string(json, "description", app->decision.description, sizeof(app->decision.description));
    if(!extract_json_string(json, "decision_type", app->decision.decision_type,
                            sizeof(app->decision.decision_type)))
        copy_text(app->decision.decision_type, sizeof(app->decision.decision_type), "consent", strlen("consent"));
    extract_json_int(json, "quorum_percent", &app->decision.quorum_percent);
    extract_json_bool(json, "require_vote_reason", &app->decision.require_vote_reason);
    extract_json_string(json, "outcome", app->decision.outcome, sizeof(app->decision.outcome));
    extract_json_string(json, "review_at", app->decision.review_at, sizeof(app->decision.review_at));
    if(extract_json_int(json, "proposal_minutes", &total_minutes)) {
        app->decision.proposal_days = total_minutes / (24 * 60);
        app->decision.proposal_hours = (total_minutes / 60) % 24;
        app->decision.proposal_minutes = total_minutes % 60;
    }
    if(extract_json_int(json, "voting_minutes", &total_minutes)) {
        app->decision.voting_days = total_minutes / (24 * 60);
        app->decision.voting_hours = (total_minutes / 60) % 24;
        app->decision.voting_minutes = total_minutes % 60;
    }
    extract_json_int(json, "negative_weight", &app->decision.negative_weight);
    if(extract_json_string(json, "created_at", created_at, sizeof(created_at)))
        app->decision.created_at = parse_lyra_time(created_at);
    if(text != NULL)
        load_default_proposals(app, text);
    else
        proposal_reset_totals(app);
    parse_process_proposals(app, json);
    parse_process_votes(app, json);
    app->process_detail_loaded = 1;
    app->process_detail_loading_failed = 0;
}

static int
lyra_fetch_process_detail(UkuApp *app, const char *base_url, const UkuText *text)
{
    char path[128];
    char url[512];
    UkuHttpBuffer response = {0};
    long status = 0;
    int ok = 0;

    if(app == NULL || app->decision.id[0] == '\0' || app->process_detail_loaded)
        return app != NULL && app->process_detail_loaded;
    if(!sync_url_valid(base_url))
        return 0;
    snprintf(path, sizeof(path), "/api/v1/governance/processes/%s", app->decision.id);
    join_url(url, sizeof(url), base_url, path);
    if(lyra_http_request("GET", url, NULL, NULL, &status, &response) && status == 200 &&
       response.data != NULL) {
        parse_process_detail(app, response.data, text);
        ok = 1;
    }
    free(response.data);
    app->process_detail_loading_failed = !ok;
    return ok;
}

static int
lyra_update_process_visibility(UkuApp *app, const char *base_url, const char *visibility)
{
    char path[128];
    UkuHttpBuffer body = {0};
    UkuHttpBuffer response = {0};
    int ok = 0;

    if(app == NULL || visibility == NULL || app->decision.id[0] == '\0' || !app->account.loaded)
        return 0;
    if(!http_buffer_append(&body, "{\"user_id_hash\":", strlen("{\"user_id_hash\":")) ||
       !json_append_string(&body, app->account.public_id) ||
       !http_buffer_append(&body, ",\"visibility\":", strlen(",\"visibility\":")) ||
       !json_append_string(&body, visibility) ||
       !http_buffer_append(&body, "}", 1))
        goto cleanup;
    snprintf(path, sizeof(path), "/api/v1/governance/processes/%s", app->decision.id);
    ok = lyra_authorized_json(app, base_url, "PATCH", path, body.data, 200, 299, &response);
    if(ok && response.data != NULL)
        parse_process_detail(app, response.data, NULL);

cleanup:
    free(body.data);
    free(response.data);
    return ok;
}

static int
lyra_delete_process(UkuApp *app, const char *base_url)
{
    char path[128];
    UkuHttpBuffer response = {0};
    int ok;

    if(app == NULL || app->decision.id[0] == '\0' || !app->account.loaded)
        return 0;
    snprintf(path, sizeof(path), "/api/v1/governance/processes/%s", app->decision.id);
    ok = lyra_authorized_json(app, base_url, "DELETE", path, NULL, 200, 299, &response);
    free(response.data);
    return ok;
}

static int
lyra_export_process(UkuApp *app, const char *base_url)
{
    char path[128];
    char url[512];
    UkuHttpBuffer response = {0};
    long status = 0;
    int ok = 0;

    if(app == NULL || app->decision.id[0] == '\0')
        return 0;
    snprintf(path, sizeof(path), "/api/v1/governance/processes/%s/export", app->decision.id);
    join_url(url, sizeof(url), base_url, path);
    if(lyra_http_request("GET", url, NULL, NULL, &status, &response) && status == 200 &&
       response.data != NULL) {
        SetClipboardText(response.data);
        ok = 1;
    }
    free(response.data);
    return ok;
}

static int
lyra_delete_proposal(UkuApp *app, const char *base_url, const char *proposal_id)
{
    char path[180];
    UkuHttpBuffer response = {0};
    int ok;

    if(app == NULL || proposal_id == NULL || app->decision.id[0] == '\0' || !app->account.loaded)
        return 0;
    snprintf(path, sizeof(path), "/api/v1/governance/processes/%s/proposals/%s",
             app->decision.id, proposal_id);
    ok = lyra_authorized_json(app, base_url, "DELETE", path, NULL, 200, 299, &response);
    if(ok && response.data != NULL)
        parse_process_detail(app, response.data, NULL);
    free(response.data);
    return ok;
}

static int
lyra_submit_proposal(UkuApp *app, const char *base_url)
{
    char path[128];
    UkuHttpBuffer body = {0};
    UkuHttpBuffer response = {0};
    int ok = 0;

    if(app == NULL || app->decision.id[0] == '\0' || !has_non_space(app->proposal_title))
        return 0;
    if(!http_buffer_append(&body, "{\"user_id_hash\":", strlen("{\"user_id_hash\":")) ||
       !json_append_string(&body, app->account.public_id) ||
       !http_buffer_append(&body, ",\"title\":", strlen(",\"title\":")) ||
       !json_append_string(&body, app->proposal_title) ||
       !http_buffer_append(&body, ",\"description\":", strlen(",\"description\":")) ||
       !json_append_string(&body, app->proposal_description) ||
       !http_buffer_append(&body, "}", 1))
        goto cleanup;
    snprintf(path, sizeof(path), "/api/v1/governance/processes/%s/proposals", app->decision.id);
    ok = lyra_authorized_json(app, base_url, "POST", path, body.data, 200, 299, &response);
    if(ok && response.data != NULL) {
        parse_process_detail(app, response.data, NULL);
        app->proposal_title[0] = '\0';
        app->proposal_description[0] = '\0';
    }

cleanup:
    free(body.data);
    free(response.data);
    return ok;
}

static int
lyra_submit_vote(UkuApp *app, const char *base_url)
{
    char path[128];
    UkuHttpBuffer body = {0};
    UkuHttpBuffer response = {0};
    char tmp[64];
    int ok = 0;

    if(app == NULL || app->decision.id[0] == '\0' || app->proposal_count <= 0)
        return 0;
    if(!http_buffer_append(&body, "{\"user_id_hash\":", strlen("{\"user_id_hash\":")) ||
       !json_append_string(&body, app->account.public_id) ||
       !http_buffer_append(&body, ",\"display_name\":", strlen(",\"display_name\":")) ||
       !json_append_string(&body, app->alias_input[0] != '\0' ? app->alias_input : app->account.public_id) ||
       !http_buffer_append(&body, ",\"scores\":{", strlen(",\"scores\":{")))
        goto cleanup;
    for(int i = 0; i < app->proposal_count; i++) {
        if(i > 0 && !http_buffer_append(&body, ",", 1))
            goto cleanup;
        if(!json_append_string(&body, app->proposals[i].id))
            goto cleanup;
        snprintf(tmp, sizeof(tmp), ":%d", app->proposals[i].score);
        if(!http_buffer_append(&body, tmp, strlen(tmp)))
            goto cleanup;
    }
    if(!http_buffer_append(&body, "},\"reason\":", strlen("},\"reason\":")) ||
       !json_append_string(&body, app->vote_reason) ||
       !http_buffer_append(&body, "}", 1))
        goto cleanup;
    snprintf(path, sizeof(path), "/api/v1/governance/processes/%s/votes", app->decision.id);
    ok = lyra_authorized_json(app, base_url, "POST", path, body.data, 200, 299, &response);
    if(ok && response.data != NULL)
        parse_process_detail(app, response.data, NULL);

cleanup:
    free(body.data);
    free(response.data);
    return ok;
}

static int
lyra_register_alias(UkuApp *app, const char *base_url, const char *alias)
{
    char url[512];
    char user_header[96];
    char auth_header[900];
    char saved_alias[40];
    UkuHttpBuffer body = {0};
    UkuHttpBuffer response = {0};
    UkuHttpHeaders *headers = NULL;
    long status = 0;
    int ok = 0;

    if(app == NULL || !app->account.loaded || !alias_valid(alias))
        return 0;
    app->account.auth_token[0] = '\0';
    account_save(app, &app->account);
    if(!lyra_login(app, base_url))
        return 0;
    if(!http_buffer_append(&body, "{\"user_id_hash\":", strlen("{\"user_id_hash\":")) ||
       !json_append_string(&body, app->account.public_id) ||
       !http_buffer_append(&body, ",\"alias\":", strlen(",\"alias\":")) ||
       !json_append_string(&body, alias) ||
       !http_buffer_append(&body, "}", 1))
        goto cleanup;

    join_url(url, sizeof(url), base_url, "/api/v1/account/alias");
    snprintf(user_header, sizeof(user_header), "X-Inbe-User: %s", app->account.public_id);
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", app->account.auth_token);
    headers = http_headers_append(headers, "Content-Type: application/json");
    headers = http_headers_append(headers, user_header);
    headers = http_headers_append(headers, auth_header);
    if(!lyra_http_request("POST", url, headers, body.data, &status, &response) ||
       status < 200 || status >= 300)
        goto cleanup;
    if(extract_json_string(response.data, "alias", saved_alias, sizeof(saved_alias)) &&
       saved_alias[0] != '\0') {
        setting_save_text(app, UKU_SYNC_ACCOUNT_ALIAS_KEY, saved_alias);
        ok = 1;
    }

cleanup:
    if(headers != NULL)
        http_headers_free(headers);
    free(body.data);
    free(response.data);
    return ok;
}

static void
lyra_fetch_public_processes(UkuApp *app, const char *base_url)
{
    char url[512];
    UkuHttpBuffer response = {0};
    long status = 0;
    const char *p;

    if(app == NULL || app->remote_processes_loaded)
        return;
    app->remote_processes_loaded = 1;
    if(!sync_url_valid(base_url))
        return;
    join_url(url, sizeof(url), base_url, "/api/v1/governance/processes");
    if(!lyra_http_request("GET", url, NULL, NULL, &status, &response) || status != 200 ||
       response.data == NULL)
        goto cleanup;

    p = response.data;
    while(app->process_count < UKU_MAX_PROCESSES && (p = strchr(p, '{')) != NULL) {
        const char *end = strchr(p, '}');
        UkuProcessRow row;
        char created_at[64];

        if(end == NULL)
            break;
        memset(&row, 0, sizeof(row));
        if(!extract_json_string(p, "id", row.id, sizeof(row.id)) ||
           !extract_json_string(p, "question", row.topic, sizeof(row.topic))) {
            p = end + 1;
            continue;
        }
        extract_json_string(p, "description", row.description, sizeof(row.description));
        extract_json_int(p, "proposal_minutes", &row.proposal_minutes);
        extract_json_int(p, "voting_minutes", &row.voting_minutes);
        extract_json_int(p, "negative_weight", &row.negative_weight);
        snprintf(row.local_address, sizeof(row.local_address), "/app/%s/collect", row.id);
        if(extract_json_string(p, "created_at", created_at, sizeof(created_at)))
            row.created_at = parse_lyra_time(created_at);
        else
            row.created_at = (sqlite3_int64)time(NULL);
        app->processes[app->process_count++] = row;
        p = end + 1;
    }

cleanup:
    free(response.data);
}

static void
account_refresh_alias_once(UkuApp *app)
{
    char alias[40];

    if(app == NULL || !app->account.loaded || app->account_alias_lookup_attempted)
        return;
    setting_load_text(app, UKU_SYNC_ACCOUNT_ALIAS_KEY, "", alias, sizeof(alias));
    if(alias[0] != '\0')
        return;
    app->account_alias_lookup_attempted = 1;
    app->account.auth_token[0] = '\0';
    account_save(app, &app->account);
    lyra_login(app, app->server_url);
}
static void
account_open_alias_modal(UkuApp *app)
{
    if(app == NULL)
        return;
    setting_load_text(app, UKU_SYNC_ACCOUNT_ALIAS_KEY, "", app->alias_input, sizeof(app->alias_input));
    alias_normalize(app->alias_input);
    app->account_public_id_modal_open = 0;
    app->account_alias_modal_open = 1;
    app->active_field = UKU_FIELD_ALIAS;
}

static void
account_open_public_id_modal(UkuApp *app)
{
    if(app == NULL)
        return;
    app->account_public_id_modal_open = 1;
    app->active_field = UKU_FIELD_NONE;
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
#if defined(PLATFORM_WEB)
    (void)db;
    (void)process_id;
    (void)title;
    (void)description;
    (void)created_at;
    return 1;
#else
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
#endif
}

static int
db_save_process(UkuApp *app, const UkuText *text)
{
#if defined(PLATFORM_WEB)
    UkuDecision *d = &app->decision;
    sqlite3_int64 now = (sqlite3_int64)time(NULL);
    UkuProcessRow row = {0};

    (void)text;
    if(app->db == NULL)
        return 0;
    generate_process_id(d->id, sizeof(d->id));
    snprintf(d->local_address, sizeof(d->local_address), "/app/%s/collect", d->id);
    d->created_at = now;
    copy_text(d->visibility, sizeof(d->visibility), "public", strlen("public"));

    copy_text(row.id, sizeof(row.id), d->id, strlen(d->id));
    copy_text(row.local_address, sizeof(row.local_address), d->local_address, strlen(d->local_address));
    copy_text(row.topic, sizeof(row.topic), d->topic, strlen(d->topic));
    copy_text(row.description, sizeof(row.description), d->description, strlen(d->description));
    row.proposal_minutes = duration_minutes(d->proposal_days, d->proposal_hours, d->proposal_minutes);
    row.voting_minutes = duration_minutes(d->voting_days, d->voting_hours, d->voting_minutes);
    row.negative_weight = d->negative_weight;
    row.created_at = now;

    if(app->process_count < UKU_MAX_PROCESSES)
        app->process_count++;
    if(app->process_count > 1)
        memmove(&app->processes[1], &app->processes[0],
                sizeof(app->processes[0]) * (size_t)(app->process_count - 1));
    app->processes[0] = row;
    return 1;
#else
    UkuDecision *d = &app->decision;
    sqlite3_stmt *stmt = NULL;
    sqlite3_int64 now = (sqlite3_int64)time(NULL);
    int ok = 0;

    if(app->db == NULL)
        return 0;

    generate_process_id(d->id, sizeof(d->id));
    snprintf(d->local_address, sizeof(d->local_address), "/app/%s/collect", d->id);
    d->created_at = now;
    copy_text(d->visibility, sizeof(d->visibility), "public", strlen("public"));

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
#endif
}

static void
db_load_processes(UkuApp *app)
{
#if defined(PLATFORM_WEB)
    (void)app;
#else
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
#endif
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
    d->require_vote_reason = 1;
    d->created_at = row->created_at;
    d->submitted = 1;
    copy_text(d->visibility, sizeof(d->visibility), "public", strlen("public"));
    copy_text(d->decision_type, sizeof(d->decision_type), "consent", strlen("consent"));
    proposals_clear(app);
    app->process_detail_loaded = 0;
    app->process_detail_loading_failed = 0;
    app->proposal_submit_failed = 0;
    app->proposal_submit_ok = 0;
    app->vote_submit_failed = 0;
    app->vote_submit_ok = 0;
    app->process_update_failed = 0;
    app->process_export_failed = 0;
    app->collect_scroll = 0;
    app->collect_max_scroll = 0;
    app->process_status[0] = '\0';
    app->proposal_title[0] = '\0';
    app->proposal_description[0] = '\0';
    app->vote_reason[0] = '\0';
    app->screen = UKU_SCREEN_COLLECT;
    app->active_field = UKU_FIELD_NONE;
    ClearUIFocus();
}

static int
process_id_char_valid(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '.' || c == '_' || c == ':' || c == '-';
}

static int
extract_process_id(const char *input, char *out, size_t out_size)
{
    const char *start;
    const char *end;
    size_t len;

    if(input == NULL || out == NULL || out_size == 0)
        return 0;
    while(*input == ' ' || *input == '\t' || *input == '\n' || *input == '\r')
        input++;
    start = strstr(input, "/app/");
    if(start != NULL) {
        start += 5;
        end = strchr(start, '/');
        if(end == NULL)
            end = start + strlen(start);
    } else {
        start = input;
        end = start + strlen(start);
    }
    while(end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' ||
                          end[-1] == '\r' || end[-1] == '/' || end[-1] == '#'))
        end--;
    len = (size_t)(end - start);
    if(len < 4 || len >= out_size)
        return 0;
    for(size_t i = 0; i < len; i++) {
        if(!process_id_char_valid(start[i]))
            return 0;
    }
    memcpy(out, start, len);
    out[len] = '\0';
    return 1;
}

static void
open_process_id(UkuApp *app, const char *id)
{
    UkuDecision *d = &app->decision;

    memset(d, 0, sizeof(*d));
    copy_text(d->id, sizeof(d->id), id, strlen(id));
    snprintf(d->local_address, sizeof(d->local_address), "/app/%s/collect", d->id);
    copy_text(d->visibility, sizeof(d->visibility), "public", strlen("public"));
    copy_text(d->decision_type, sizeof(d->decision_type), "consent", strlen("consent"));
    d->require_vote_reason = 1;
    d->created_at = (sqlite3_int64)time(NULL);
    d->submitted = 1;
    proposals_clear(app);
    app->process_detail_loaded = 0;
    app->process_detail_loading_failed = 0;
    app->proposal_submit_failed = 0;
    app->proposal_submit_ok = 0;
    app->vote_submit_failed = 0;
    app->vote_submit_ok = 0;
    app->process_update_failed = 0;
    app->process_export_failed = 0;
    app->collect_scroll = 0;
    app->collect_max_scroll = 0;
    app->process_status[0] = '\0';
    app->proposal_title[0] = '\0';
    app->proposal_description[0] = '\0';
    app->vote_reason[0] = '\0';
    app->screen = UKU_SCREEN_COLLECT;
    app->active_field = UKU_FIELD_NONE;
    ClearUIFocus();
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
    int focused = RegisterUIFocus(focus_id, bounds);
    Color fill = primary ? GetThemeButton() : (Color){255, 255, 255, 255};
    Color text = primary ? WHITE : GetThemeText();
    int font_size = ClampUIPx(16, 16, 20);

    if(hover || focused) {
        fill = primary ? LightenUIColor(GetThemeButton(), 18) : (Color){242, 245, 247, 255};
        if(hover)
            app->cursor_clickable = 1;
    }

    DrawRectangleRounded(bounds, 0.12f, 12, fill);
    DrawRectangleRoundedLinesEx(bounds, 0.12f, 12, ScaleUIPx(1), primary ? DarkenUIColor(GetThemeButton(), 20) : GetThemeText());
    if(focused)
        DrawUIFocus(bounds);
    draw_centered_text(font, label, x + w / 2,
                       GetUIControlTextY(label, y, h, font_size),
                       font_size, text);

    *clicked = (hover && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) || IsUIFocusActivatePressed(focus_id);
}

static int
draw_readonly_field(UkuApp *app, Font font, const char *text, int x, int y, int w, int h,
                    int focus_id, int *clicked)
{
    Vector2 mouse = GetMousePosition();
    Rectangle box = {(float)x, (float)y, (float)w, (float)h};
    int hover = CheckCollisionPointRec(mouse, box);
    int focused = RegisterUIFocus(focus_id, box);
    int pad = ScaleUIPx(12);
    int text_font = ClampUIPx(15, 15, 19);

    if(hover)
        app->cursor_clickable = 1;
    DrawRectangleRounded(box, 0.08f, 10, GetThemeSurface());
    DrawRectangleRoundedLinesEx(box, 0.08f, 10, ScaleUIPx(focused ? 2 : 1),
                                focused ? GetThemeButton() : GetThemeText());
    if(focused)
        DrawUIFocus(box);
    draw_text_font(font, fit_tail(font, text, text_font, w - pad * 2),
                   x + pad, GetUIControlTextY(text, y, h, text_font),
                   text_font, GetThemeText());
    *clicked = (hover && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) || IsUIFocusActivatePressed(focus_id);
    return y + h + ScaleUIPx(16);
}

static void
load_icons_once(UkuApp *app)
{
    if(!app->icons_loaded) {
        for(int i = 0; i < UI_ICON_TYPE_COUNT; i++) {
            app->icons[i] = LoadUIIconTexture(i);
        }
        app->icons_loaded = 1;
    }
}

static int
draw_icon_button(UkuApp *app, int x, int y, int size, UIIconType icon, int focus_id)
{
    (void)focus_id;

    Rectangle bounds = {(float)x, (float)y, (float)size, (float)size};
    Vector2 mouse = GetMousePosition();
    int hover = CheckCollisionPointRec(mouse, bounds);

    Color bg = GetThemeButton();
    Color hover_bg = GetThemeButtonHover();

    if(hover) {
        app->cursor_clickable = 1;
        DrawRectangleRounded(bounds, 0.22f, 12, hover_bg);
        if(IsMouseButtonReleased(MOUSE_LEFT_BUTTON))
            return 1;
    } else {
        DrawRectangleRounded(bounds, 0.22f, 12, bg);
    }

    Texture2D icon_texture = app->icons[icon];
    Rectangle icon_rect = {x + size/2 - icon_texture.width/2, y + size/2 - icon_texture.height/2,
                          (float)icon_texture.width, (float)icon_texture.height};
    DrawTexturePro(icon_texture, (Rectangle){0, 0, icon_texture.width, icon_texture.height},
                  icon_rect, (Vector2){0}, 0.0f, GetThemeIcon());

    return 0;
}

static void
draw_top_bar(UkuApp *app, const char *title, int show_back, int back_focus_id, int *back_clicked,
             int show_manual, int *manual_clicked, int show_settings, int *settings_clicked, int show_close, int *close_clicked, int view_w)
{
    int h = ScaleUIPx(58);
    int font_size = ClampUIPx(18, 18, 24);
    int x = ScaleUIPx(18);

    DrawRectangle(0, 0, view_w, h, GetThemeSurface());
    DrawLine(0, h - 1, view_w, h - 1, GetThemeText());

    if(show_back) {
        int clicked = draw_icon_button(app, x, ScaleUIPx(9), ScaleUIPx(28),
                                       UI_ICON_TYPE_RETURN, back_focus_id);
        if(back_clicked != NULL && clicked)
            *back_clicked = 1;
        x += ScaleUIPx(36);
    }

    draw_text_font(app->font, title, x, GetUIControlTextY(title, 0, h, font_size), font_size, GetThemeText());

    int button_x = view_w - ScaleUIPx(44);
    if(show_settings && settings_clicked != NULL) {
        int clicked = draw_icon_button(app, button_x, ScaleUIPx(9), ScaleUIPx(28),
                                       UI_ICON_TYPE_PROFILE, UKU_FOCUS_SETTINGS);
        if(clicked)
            *settings_clicked = 1;
        button_x -= ScaleUIPx(36);
    }

    if(show_manual && manual_clicked != NULL) {
        int clicked = draw_icon_button(app, button_x, ScaleUIPx(9), ScaleUIPx(28),
                                       UI_ICON_TYPE_MANUAL, UKU_FOCUS_DASHBOARD_MANUAL);
        if(clicked)
            *manual_clicked = 1;
        button_x -= ScaleUIPx(36);
    }

    if(show_close && close_clicked != NULL) {
        int clicked = draw_icon_button(app, button_x, ScaleUIPx(9), ScaleUIPx(28),
                                       UI_ICON_TYPE_RETURN, UKU_FOCUS_DASHBOARD_CLOSE);
        if(clicked)
            *close_clicked = 1;
    }
}

static int
draw_text_field(UkuApp *app, Font font, const char *label, const char *placeholder,
                char *buffer, size_t cap, UkuField field, int focus_id, int x, int y, int w, int h)
{
    int label_font = ClampUIPx(13, 13, 16);
    int input_font = ClampUIPx(16, 16, 20);
    int pad = ScaleUIPx(12);
    int label_y = y;
    int box_y = y + label_font + ScaleUIPx(8);
    Rectangle box = {(float)x, (float)box_y, (float)w, (float)h};
    Vector2 mouse = GetMousePosition();
    int focused;
    int active;

    draw_text_font(font, label, x, label_y, label_font, GetThemeText());

    if(IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if(CheckCollisionPointRec(mouse, box))
            app->active_field = field;
        else if(app->active_field == field)
            app->active_field = UKU_FIELD_NONE;
    }

    focused = RegisterUIFocus(focus_id, box);
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

    DrawRectangleRounded(box, 0.08f, 10, GetThemeSurface());
    DrawRectangleRoundedLinesEx(box, 0.08f, 10, ScaleUIPx(active ? 2 : 1),
                                active ? GetThemeButton() : GetThemeText());
    if(focused)
        DrawUIFocus(box);

    if(buffer[0] == '\0') {
        draw_text_font(font, placeholder, x + pad,
                       GetUIControlTextY(placeholder, box_y, h, input_font),
                       input_font, GetThemeText());
    } else if(h > ScaleUIPx(58)) {
        BeginScissorMode(x + pad, box_y + pad, w - pad * 2, h - pad * 2);
        draw_wrapped_text(font, buffer, x + pad, box_y + pad, w - pad * 2,
                          input_font, input_font + ScaleUIPx(7), GetThemeText());
        EndScissorMode();
    } else {
        const char *visible = fit_tail(font, buffer, input_font, w - pad * 2 - ScaleUIPx(8));
        draw_text_font(font, visible, x + pad,
                       GetUIControlTextY(visible, box_y, h, input_font),
                       input_font, GetThemeText());
    }

    if(active && ((int)(GetTime() * 2.0) % 2) == 0) {
        int cursor_x;
        if(h > ScaleUIPx(58))
            cursor_x = x + pad + UKU_MIN(measure_text_font(font, buffer, input_font), w - pad * 2 - ScaleUIPx(4));
        else
            cursor_x = x + pad + measure_text_font(font, fit_tail(font, buffer, input_font, w - pad * 2 - ScaleUIPx(8)), input_font);
        DrawLine(cursor_x, box_y + pad, cursor_x, box_y + h - pad, GetThemeButton());
    }

    return box_y + h + ScaleUIPx(16);
}

static int
draw_stepper(UkuApp *app, Font font, const char *label, int *value, int min_value, int max_value,
             int x, int y, int w, int minus_focus_id, int plus_focus_id)
{
    int label_font = ClampUIPx(13, 13, 16);
    int value_font = ClampUIPx(18, 18, 22);
    int btn = ScaleUIPx(34);
    int h = ScaleUIPx(38);
    int value_w = w - btn * 2 - ScaleUIPx(8);
    int minus_clicked = 0;
    int plus_clicked = 0;
    char value_text[16];

    draw_text_font(font, label, x, y, label_font, GetThemeText());
    y += label_font + ScaleUIPx(7);

    draw_button(app, font, x, y, btn, h, "-", 0, minus_focus_id, &minus_clicked);
    DrawRectangleRounded((Rectangle){x + btn + ScaleUIPx(4), y, value_w, h}, 0.08f, 10, GetThemeSurface());
    DrawRectangleRoundedLinesEx((Rectangle){x + btn + ScaleUIPx(4), y, value_w, h}, 0.08f, 10, ScaleUIPx(1), GetThemeText());
    snprintf(value_text, sizeof(value_text), "%d", *value);
    draw_centered_text(font, value_text, x + btn + ScaleUIPx(4) + value_w / 2,
                       y + (h - value_font) / 2, value_font, GetThemeText());
    draw_button(app, font, x + btn + ScaleUIPx(8) + value_w, y, btn, h, "+", 0, plus_focus_id, &plus_clicked);

    if(minus_clicked)
        *value = clampi(*value - 1, min_value, max_value);
    if(plus_clicked)
        *value = clampi(*value + 1, min_value, max_value);

    return y + h + ScaleUIPx(14);
}

static int
draw_negative_weight_dropdown(UkuApp *app, Font font, const UkuText *text, int x, int y, int w, int focus_id)
{
    int label_font = ClampUIPx(13, 13, 16);
    int input_font = ClampUIPx(16, 16, 20);
    int h = ScaleUIPx(40);
    int option_h = ScaleUIPx(34);
    int pad = ScaleUIPx(12);
    int box_y;
    Rectangle box;
    Vector2 mouse = GetMousePosition();
    int selected = clampi(app->decision.negative_weight, 0, 9);
    int focused;

    draw_text_font(font, text->negative_weight_label, x, y, label_font, GetThemeText());
    box_y = y + label_font + ScaleUIPx(8);
    box = (Rectangle){(float)x, (float)box_y, (float)w, (float)h};
    focused = RegisterUIFocus(focus_id, box);

    if(CheckCollisionPointRec(mouse, box))
        app->cursor_clickable = 1;
    if((CheckCollisionPointRec(mouse, box) && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) ||
       IsUIFocusActivatePressed(focus_id)) {
        app->negative_dropdown_open = !app->negative_dropdown_open;
        app->active_field = UKU_FIELD_NONE;
    }
    if(focused) {
        if(IsKeyPressed(KEY_DOWN))
            app->decision.negative_weight = clampi(app->decision.negative_weight + 1, 0, 9);
        if(IsKeyPressed(KEY_UP))
            app->decision.negative_weight = clampi(app->decision.negative_weight - 1, 0, 9);
    }

    DrawRectangleRounded(box, 0.08f, 10, GetThemeSurface());
    DrawRectangleRoundedLinesEx(box, 0.08f, 10, ScaleUIPx(1), app->negative_dropdown_open ? GetThemeButton() : GetThemeText());
    if(focused)
        DrawUIFocus(box);
    draw_text_font(font, text->negative_weight_options[selected], x + pad,
                   box_y + (h - input_font) / 2, input_font, GetThemeText());
    DrawTriangle((Vector2){(float)(x + w - pad - ScaleUIPx(10)), (float)(box_y + h / 2 - ScaleUIPx(3))},
                 (Vector2){(float)(x + w - pad), (float)(box_y + h / 2 - ScaleUIPx(3))},
                 (Vector2){(float)(x + w - pad - ScaleUIPx(5)), (float)(box_y + h / 2 + ScaleUIPx(4))},
                 GetThemeText());

    if(app->negative_dropdown_open) {
        int menu_y = box_y + h + ScaleUIPx(4);
        int menu_h = option_h * 10;
        Rectangle menu = {(float)x, (float)menu_y, (float)w, (float)menu_h};

        DrawRectangleRounded(menu, 0.06f, 10, GetThemeSurface());
        DrawRectangleRoundedLinesEx(menu, 0.06f, 10, ScaleUIPx(1), GetThemeText());
        for(int i = 0; i < 10; i++) {
            int oy = menu_y + i * option_h;
            Rectangle option = {(float)x, (float)oy, (float)w, (float)option_h};
            int hover = CheckCollisionPointRec(mouse, option);

            if(hover) {
                DrawRectangle(x + ScaleUIPx(2), oy, w - ScaleUIPx(4), option_h, (Color){238, 243, 247, 255});
                app->cursor_clickable = 1;
            }
            if(i == selected)
                DrawRectangle(x + ScaleUIPx(5), oy + ScaleUIPx(8), ScaleUIPx(4), option_h - ScaleUIPx(16), GetThemeButton());
            draw_text_font(font, text->negative_weight_options[i], x + pad + ScaleUIPx(8),
                           oy + (option_h - input_font) / 2, input_font, GetThemeText());
            if(hover && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
                app->decision.negative_weight = i;
                app->negative_dropdown_open = 0;
            }
        }

        if(IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
           !CheckCollisionPointRec(mouse, box) && !CheckCollisionPointRec(mouse, menu))
            app->negative_dropdown_open = 0;

        return menu_y + menu_h + ScaleUIPx(18);
    }

    return box_y + h + ScaleUIPx(16);
}

static void
apply_process_template(UkuDecision *d, const UkuProcessTemplate *tmpl)
{
    copy_text(d->decision_type, sizeof(d->decision_type), tmpl->type, strlen(tmpl->type));
    d->proposal_days = tmpl->proposal_days;
    d->proposal_hours = tmpl->proposal_hours;
    d->proposal_minutes = tmpl->proposal_minutes;
    d->voting_days = tmpl->voting_days;
    d->voting_hours = tmpl->voting_hours;
    d->voting_minutes = tmpl->voting_minutes;
    d->negative_weight = tmpl->negative_weight;
    d->quorum_percent = tmpl->quorum_percent;
    d->require_vote_reason = tmpl->require_vote_reason;
}

static int
process_template_selected(const UkuDecision *d, const UkuProcessTemplate *tmpl)
{
    const char *type = d->decision_type[0] != '\0' ? d->decision_type : "consent";

    return strcmp(type, tmpl->type) == 0;
}

static int
draw_process_type_selector(UkuApp *app, Font font, int x, int y, int w)
{
    int label_font = ClampUIPx(13, 13, 16);
    int input_font = ClampUIPx(16, 16, 20);
    int h = ScaleUIPx(42);
    int pad = ScaleUIPx(12);
    int box_y;
    const UkuProcessTemplate *selected = &process_templates[0];
    int count = (int)(sizeof(process_templates) / sizeof(process_templates[0]));
    Rectangle box;
    Vector2 mouse = GetMousePosition();
    int focused;
    int open;

    draw_text_font(font, "Process type", x, y, label_font, GetThemeText());
    box_y = y + label_font + ScaleUIPx(8);
    box = (Rectangle){(float)x, (float)box_y, (float)w, (float)h};

    for(int i = 0; i < count; i++) {
        if(process_template_selected(&app->decision, &process_templates[i]))
            selected = &process_templates[i];
    }

    focused = RegisterUIFocus(UKU_FOCUS_PROCESS_TYPE_BASE, box);
    if(CheckCollisionPointRec(mouse, box))
        app->cursor_clickable = 1;

    DrawRectangleRounded(box, 0.08f, 10, GetThemeSurface());
    DrawRectangleRoundedLinesEx(box, 0.08f, 10, ScaleUIPx(focused ? 2 : 1),
                                focused ? GetThemeButton() : GetThemeText());
    if(focused)
        DrawUIFocus(box);
    draw_text_font(font, fit_tail(font, selected->title, input_font,
                                  w - pad * 2 - ScaleUIPx(22)),
                   x + pad, GetUIControlTextY(selected->title, box_y, h, input_font),
                   input_font, GetThemeText());
    DrawTriangle((Vector2){(float)(x + w - pad - ScaleUIPx(10)), (float)(box_y + h / 2 - ScaleUIPx(3))},
                 (Vector2){(float)(x + w - pad), (float)(box_y + h / 2 - ScaleUIPx(3))},
                 (Vector2){(float)(x + w - pad - ScaleUIPx(5)), (float)(box_y + h / 2 + ScaleUIPx(4))},
                 GetThemeText());

    open = (CheckCollisionPointRec(mouse, box) && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) ||
           IsUIFocusActivatePressed(UKU_FOCUS_PROCESS_TYPE_BASE);
    if(open) {
        app->process_type_modal_open = 1;
        app->negative_dropdown_open = 0;
        app->active_field = UKU_FIELD_NONE;
    }

    return box_y + h + ScaleUIPx(16);
}

static void
draw_scrollbar(UkuApp *app, int x, int y, int h, int content_h, int max_scroll,
               int *scroll, int *dragging, int *drag_offset)
{
    int track_w = ScaleUIPx(8);
    int hit_w = ScaleUIPx(30);
    int hit_x = x - (hit_w - track_w) / 2;
    int thumb_h;
    int thumb_y;
    Rectangle track;
    Rectangle hit_track;
    Rectangle thumb;
    Rectangle hit_thumb;
    Vector2 mouse = GetMousePosition();

    track = (Rectangle){(float)x, (float)y, (float)track_w, (float)h};
    hit_track = (Rectangle){(float)hit_x, (float)y, (float)hit_w, (float)h};
    if(max_scroll <= 0) {
        *dragging = 0;
        return;
    }

    thumb_h = UKU_MAX(ScaleUIPx(42), (int)((float)h * (float)h / (float)content_h));
    thumb_h = UKU_MIN(thumb_h, h);
    thumb_y = y + (int)((float)(h - thumb_h) * ((float)(*scroll) / (float)max_scroll));
    thumb = (Rectangle){(float)x, (float)thumb_y, (float)track_w, (float)thumb_h};
    hit_thumb = (Rectangle){(float)hit_x, (float)thumb_y, (float)hit_w, (float)thumb_h};

    DrawRectangleRounded(track, 0.5f, 8, (Color){226, 230, 233, 255});
    DrawRectangleRounded(thumb, 0.5f, 8, GetThemeButton());

    if(CheckCollisionPointRec(mouse, hit_track))
        app->cursor_clickable = 1;

    if(IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mouse, hit_thumb)) {
        *dragging = 1;
        *drag_offset = (int)mouse.y - thumb_y;
    } else if(IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mouse, hit_track)) {
        *dragging = 1;
        *drag_offset = thumb_h / 2;
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
    int title_font = ClampUIPx(16, 16, 20);
    int gap = ScaleUIPx(10);
    int col_w = (w - gap * 2) / 3;
    int y2;

    draw_text_font(font, title, x, y, title_font, GetThemeText());
    y += title_font + ScaleUIPx(12);

    y2 = draw_stepper(app, font, text->days_label, days, 0, 30, x, y, col_w, focus_base, focus_base + 1);
    draw_stepper(app, font, text->hours_label, hours, 0, 23, x + col_w + gap, y, col_w, focus_base + 2, focus_base + 3);
    draw_stepper(app, font, text->minutes_label, minutes, 0, 59, x + (col_w + gap) * 2, y, col_w, focus_base + 4, focus_base + 5);
    return y2 + ScaleUIPx(6);
}

static void
init_decision(UkuApp *app, const UkuText *text)
{
    UkuDecision *d = &app->decision;

    copy_text(d->visibility, sizeof(d->visibility), "public", strlen("public"));
    apply_process_template(d, &process_templates[0]);
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

static UkuProcessPhase
process_phase(sqlite3_int64 created_at, int proposal_minutes, int voting_minutes,
              sqlite3_int64 now, sqlite3_int64 *remaining_seconds)
{
    sqlite3_int64 proposal_end = created_at + (sqlite3_int64)proposal_minutes * 60;
    sqlite3_int64 voting_end = proposal_end + (sqlite3_int64)voting_minutes * 60;

    if(now < proposal_end) {
        if(remaining_seconds != NULL)
            *remaining_seconds = proposal_end - now;
        return UKU_PROCESS_PROPOSAL;
    }
    if(now < voting_end) {
        if(remaining_seconds != NULL)
            *remaining_seconds = voting_end - now;
        return UKU_PROCESS_VOTING;
    }

    if(remaining_seconds != NULL)
        *remaining_seconds = 0;
    return UKU_PROCESS_RESULTS;
}

static const char *
phase_label(const UkuText *text, UkuProcessPhase phase)
{
    if(phase == UKU_PROCESS_PROPOSAL)
        return text->proposal_phase_label;
    if(phase == UKU_PROCESS_VOTING)
        return text->voting_phase_label;
    return text->results_phase_label;
}

static void
format_duration_compact(char *dst, size_t size, sqlite3_int64 seconds)
{
    sqlite3_int64 days;
    sqlite3_int64 hours;
    sqlite3_int64 minutes;

    if(seconds < 0)
        seconds = 0;

    days = seconds / 86400;
    seconds %= 86400;
    hours = seconds / 3600;
    seconds %= 3600;
    minutes = seconds / 60;
    seconds %= 60;

    if(days > 0)
        snprintf(dst, size, "%lldd %lldh", (long long)days, (long long)hours);
    else if(hours > 0)
        snprintf(dst, size, "%lldh %lldm", (long long)hours, (long long)minutes);
    else if(minutes > 0)
        snprintf(dst, size, "%lldm %llds", (long long)minutes, (long long)seconds);
    else
        snprintf(dst, size, "%llds", (long long)seconds);
}

static void
append_text(char *dst, size_t size, const char *src)
{
    size_t len;

    if(dst == NULL || size == 0 || src == NULL)
        return;

    len = strlen(dst);
    if(len >= size - 1)
        return;

    copy_text(dst + len, size - len, src, strlen(src));
}

static void
format_process_timer(char *dst, size_t size, const UkuText *text, sqlite3_int64 created_at,
                     int proposal_minutes, int voting_minutes, sqlite3_int64 now)
{
    sqlite3_int64 remaining = 0;
    UkuProcessPhase phase = process_phase(created_at, proposal_minutes, voting_minutes, now, &remaining);
    char duration[48];

    if(phase == UKU_PROCESS_RESULTS) {
        copy_text(dst, size, phase_label(text, phase), strlen(phase_label(text, phase)));
        return;
    }

    format_duration_compact(duration, sizeof(duration), remaining);
    copy_text(dst, size, phase_label(text, phase), strlen(phase_label(text, phase)));
    append_text(dst, size, " | ");
    append_text(dst, size, duration);
    append_text(dst, size, " ");
    append_text(dst, size, text->time_remaining_label);
}

static void
draw_home(UkuApp *app, const UkuText *text, int view_w, int view_h)
{
    int side = GetUIPageSidePadding();
    int content_x;
    int content_w;
    int top_h = ScaleUIPx(58);
    int title_font = ClampUIPx(22, 22, 30);
    int body_font = ClampUIPx(16, 16, 20);
    int small_font = ClampUIPx(13, 13, 16);
    int line_h = body_font + ScaleUIPx(8);
    int viewport_y = top_h;
    int viewport_h = view_h - viewport_y;
    int y = viewport_y + ScaleUIPx(20) - app->dashboard_scroll;
    int content_bottom;
    int content_h;
    int profile_clicked = 0;
    int new_clicked = 0;
    int join_clicked = 0;
    Font font = app->font;
    sqlite3_int64 now = (sqlite3_int64)time(NULL);

    if(!app->remote_processes_loaded) {
        db_load_processes(app);
        app->remote_processes_loaded = 1;
    }
    GetUICenteredColumn(760, side, &content_x, &content_w);
    app->dashboard_scroll = clampi(app->dashboard_scroll - (int)(GetMouseWheelMove() * ScaleUIPx(44)),
                                   0, app->dashboard_max_scroll);

    draw_top_bar(app, text->app_title, 0, 0, NULL, 0, NULL, 1, &profile_clicked, 0, NULL, view_w);
    if(profile_clicked) {
        app->screen = UKU_SCREEN_ACCOUNT;
        ClearUIFocus();
    }

    BeginScissorMode(0, viewport_y, view_w, viewport_h);
    draw_text_font(font, text->home_subtitle, content_x, y, title_font, GetThemeText());
    y += title_font + ScaleUIPx(12);
    y = draw_wrapped_text(font, text->home_summary, content_x, y, content_w, body_font, line_h, GetThemeText());
    y += ScaleUIPx(20);

    {
        int card_h = ScaleUIPx(86);
        Rectangle card = {(float)content_x, (float)y, (float)content_w, (float)card_h};
        Vector2 mouse = GetMousePosition();
        int hovered = CheckCollisionPointRec(mouse, card);
        int focused = RegisterUIFocus(UKU_FOCUS_DASHBOARD_NEW, card);
        int icon_size = ScaleUIPx(34);
        Texture2D icon_texture;

        if(hovered)
            app->cursor_clickable = 1;
        DrawRectangleRounded(card, 0.07f, 10, GetThemeSurface());
        DrawRectangleRoundedLinesEx(card, 0.07f, 10, ScaleUIPx(focused ? 2 : 1),
                                    focused ? GetThemeButton() : GetThemeText());
        if(focused)
            DrawUIFocus(card);

        icon_texture = app->icons[UI_ICON_TYPE_PLUS];
        if(icon_texture.id != 0) {
            Rectangle icon_rect = {(float)(content_x + ScaleUIPx(16)),
                                   (float)(y + (card_h - icon_size) / 2),
                                   (float)icon_size, (float)icon_size};
            DrawTexturePro(icon_texture, (Rectangle){0, 0, (float)icon_texture.width, (float)icon_texture.height},
                           icon_rect, (Vector2){0}, 0.0f, GetThemeIcon());
        }
        draw_text_font(font, text->start_process_button, content_x + ScaleUIPx(64),
                       y + ScaleUIPx(18), body_font, GetThemeText());
        draw_text_font(font, text->topic_question_placeholder, content_x + ScaleUIPx(64),
                       y + ScaleUIPx(46), small_font, GetThemeText());

        new_clicked = (hovered && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) ||
                      IsUIFocusActivatePressed(UKU_FOCUS_DASHBOARD_NEW);
        if(new_clicked) {
            reset_decision(app, text);
            app->screen = UKU_SCREEN_CREATE;
            ClearUIFocus();
        }
        y += card_h + ScaleUIPx(24);
    }

    y = draw_text_field(app, font, text->join_process_label, text->join_process_placeholder,
                        app->join_process_input, sizeof(app->join_process_input),
                        UKU_FIELD_JOIN_PROCESS, UKU_FOCUS_JOIN_PROCESS,
                        content_x, y, content_w, ScaleUIPx(46));
    draw_button(app, font, content_x, y, content_w, ScaleUIPx(42),
                text->join_process_button, 0, UKU_FOCUS_JOIN_PROCESS_OPEN, &join_clicked);
    if(join_clicked) {
        char process_id[40];

        app->join_process_failed = !extract_process_id(app->join_process_input,
                                                       process_id, sizeof(process_id));
        if(!app->join_process_failed)
            open_process_id(app, process_id);
    }
    y += ScaleUIPx(56);
    if(app->join_process_failed) {
        y = draw_wrapped_text(font, text->join_process_error, content_x, y, content_w,
                              small_font, line_h, GetThemeButton());
        y += ScaleUIPx(16);
    }

    draw_text_font(font, text->dashboard_recent_label, content_x, y, title_font, GetThemeText());
    y += title_font + ScaleUIPx(18);

    if(app->process_count <= 0) {
        y = draw_wrapped_text(font, text->dashboard_empty, content_x, y, content_w, body_font, line_h, GetThemeText());
    } else {
        int card_h = ScaleUIPx(118);
        int gap = ScaleUIPx(12);
        for(int i = 0; i < app->process_count; i++) {
            UkuProcessRow *row = &app->processes[i];
            Rectangle card = {(float)content_x, (float)y, (float)content_w, (float)card_h};
            Vector2 mouse = GetMousePosition();
            int focus_id = UKU_FOCUS_DASHBOARD_PROCESS_BASE + i;
            int hovered = CheckCollisionPointRec(mouse, card);
            int focused = RegisterUIFocus(focus_id, card);
            char meta[160];
            char created[32];
            char timer[128];
            int open = 0;

            if(y + card_h < viewport_y || y > viewport_y + viewport_h) {
                y += card_h + gap;
                continue;
            }

            if(hovered)
                app->cursor_clickable = 1;
            DrawRectangleRounded(card, 0.07f, 10, hovered || focused ? GetThemeSurface() : GetThemeSurface());
            DrawRectangleRoundedLinesEx(card, 0.07f, 10, ScaleUIPx(1), focused ? GetThemeButton() : GetThemeText());
            if(focused)
                DrawUIFocus(card);

            draw_text_font(font, fit_tail(font, row->topic, body_font, content_w - ScaleUIPx(28)),
                           content_x + ScaleUIPx(14), y + ScaleUIPx(12), body_font, GetThemeText());
            format_process_timer(timer, sizeof(timer), text, row->created_at,
                                 row->proposal_minutes, row->voting_minutes, now);
            draw_text_font(font, fit_tail(font, timer, small_font, content_w - ScaleUIPx(28)),
                           content_x + ScaleUIPx(14), y + ScaleUIPx(40), small_font,
                           process_phase(row->created_at, row->proposal_minutes, row->voting_minutes, now, NULL) == UKU_PROCESS_RESULTS ? GetThemeText() : GetThemeButton());
            format_created_at(created, sizeof(created), row->created_at);
            snprintf(meta, sizeof(meta), "%s  |  %s", row->local_address, created);
            draw_text_font(font, fit_tail(font, meta, small_font, content_w - ScaleUIPx(28)),
                           content_x + ScaleUIPx(14), y + ScaleUIPx(66), small_font, GetThemeButton());
            if(row->description[0] != '\0')
                draw_text_font(font, fit_tail(font, row->description, small_font, content_w - ScaleUIPx(28)),
                               content_x + ScaleUIPx(14), y + ScaleUIPx(90), small_font, GetThemeText());

            open = (hovered && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) || IsUIFocusActivatePressed(focus_id);
            if(open)
                open_process_row(app, row);
            y += card_h + gap;
        }
    }
    EndScissorMode();

    content_bottom = y + app->dashboard_scroll + ScaleUIPx(24);
    content_h = content_bottom - viewport_y;
    app->dashboard_max_scroll = UKU_MAX(0, content_h - viewport_h);
    app->dashboard_scroll = clampi(app->dashboard_scroll, 0, app->dashboard_max_scroll);
    draw_scrollbar(app, view_w - side - ScaleUIPx(8), viewport_y + ScaleUIPx(8),
                   viewport_h - ScaleUIPx(16), content_h, app->dashboard_max_scroll,
                   &app->dashboard_scroll, &app->dashboard_drag_scrollbar, &app->dashboard_scroll_drag_offset);
}

static void
draw_create_placeholder(UkuApp *app, const UkuText *text, int view_w, int view_h)
{
    int side = GetUIPageSidePadding();
    int content_x;
    int content_w;
    int small_font = ClampUIPx(13, 13, 16);
    int top_h = ScaleUIPx(58);
    int y = top_h + ScaleUIPx(20) - app->create_scroll;
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

    app->create_scroll = clampi(app->create_scroll - (int)(GetMouseWheelMove() * ScaleUIPx(44)),
                                0, app->create_max_scroll);
    GetUICenteredColumn(680, side, &content_x, &content_w);
    if(reserve_scrollbar)
        content_w = UKU_MAX(ScaleUIPx(220), GetUIScrollbarContentWidth(content_w, 1));

    draw_top_bar(app, text->create_title, 1, UKU_FOCUS_CREATE_BACK, &back_clicked, 0, NULL, 0, NULL, 0, NULL, view_w);
    if(back_clicked) {
        app->screen = UKU_SCREEN_HOME;
        app->active_field = UKU_FIELD_NONE;
        ClearUIFocus();
    }

    BeginScissorMode(0, viewport_y, view_w, viewport_h);
    y = draw_process_type_selector(app, font, content_x, y, content_w);
    y = draw_text_field(app, font, text->topic_question_label, text->topic_question_placeholder,
                        d->topic, sizeof(d->topic), UKU_FIELD_TOPIC, UKU_FOCUS_TOPIC,
                        content_x, y, content_w, ScaleUIPx(46));
    if(d->topic_error) {
        draw_text_font(font, text->topic_error, content_x, y - ScaleUIPx(8), small_font, GetThemeButton());
        y += small_font + ScaleUIPx(8);
    }
    y = draw_text_field(app, font, text->description_label, text->description_placeholder,
                        d->description, sizeof(d->description), UKU_FIELD_DESCRIPTION, UKU_FOCUS_DESCRIPTION,
                        content_x, y, content_w, ScaleUIPx(82));
    y = draw_negative_weight_dropdown(app, font, text, content_x, y, UKU_MIN(content_w, ScaleUIPx(310)),
                                      UKU_FOCUS_NEGATIVE_WEIGHT);
    y += ScaleUIPx(6);
    y = draw_duration_group(app, font, text->proposal_time_label, text,
                            &d->proposal_days, &d->proposal_hours, &d->proposal_minutes,
                            content_x, y, content_w, UKU_FOCUS_PROPOSAL_DAYS_MINUS);
    y = draw_duration_group(app, font, text->voting_time_label, text,
                            &d->voting_days, &d->voting_hours, &d->voting_minutes,
                            content_x, y, content_w, UKU_FOCUS_VOTING_DAYS_MINUS);

    draw_button(app, font, content_x, y, content_w, ScaleUIPx(48), text->create_process_button, 1,
                UKU_FOCUS_CREATE_SUBMIT, &clicked);
    if(clicked) {
        d->topic_error = !has_non_space(d->topic);
        d->db_error = 0;
        d->remote_error = 0;
        if(!d->topic_error) {
            d->submitted = db_save_process(app, text);
            d->db_error = !d->submitted;
            if(d->submitted) {
                d->remote_error = !lyra_create_process(app, app->server_url);
                app->remote_processes_loaded = 0;
                app->process_detail_loaded = 0;
                app->process_detail_loading_failed = 0;
                app->proposal_submit_failed = 0;
                app->proposal_submit_ok = 0;
                app->vote_submit_failed = 0;
                app->vote_submit_ok = 0;
                app->process_export_failed = 0;
                app->collect_scroll = 0;
                app->collect_max_scroll = 0;
                app->process_status[0] = '\0';
                app->vote_reason[0] = '\0';
                load_default_proposals(app, text);
                app->screen = UKU_SCREEN_COLLECT;
                app->active_field = UKU_FIELD_NONE;
                ClearUIFocus();
            }
        }
    }
    y += ScaleUIPx(62);
    if(d->submitted)
        draw_centered_text(font, text->setup_ready, content_x + content_w / 2, y, small_font, GetThemeButton());
    if(d->db_error)
        draw_centered_text(font, text->db_error, content_x + content_w / 2, y, small_font, GetThemeButton());
    if(d->remote_error)
        draw_centered_text(font, "Saved locally, but Lyra upload failed.", content_x + content_w / 2, y + ScaleUIPx(20), small_font, GetThemeButton());
    EndScissorMode();

    content_bottom = y + app->create_scroll + ScaleUIPx(24);
    content_h = content_bottom - viewport_y;
    max_scroll = UKU_MAX(0, content_h - viewport_h);
    app->create_max_scroll = max_scroll;
    app->create_scrollbar_visible = max_scroll > 0;
    app->create_scroll = clampi(app->create_scroll, 0, max_scroll);
    draw_scrollbar(app, view_w - side - ScaleUIPx(8), viewport_y + ScaleUIPx(8),
                   viewport_h - ScaleUIPx(16), content_h, max_scroll,
                   &app->create_scroll, &app->create_scroll_dragging, &app->create_scroll_drag_offset);
    (void)start_y;
}

static int
draw_proposal_card(UkuApp *app, Font font, const UkuProposal *proposal,
                   int index, int x, int y, int w, int body_font, int small_font)
{
    int line_h = small_font + ScaleUIPx(7);
    int h = ScaleUIPx(66);
    int can_delete;
    int delete_clicked = 0;
    Rectangle card;

    if(proposal->description[0] != '\0')
        h += ScaleUIPx(24);
    can_delete = app->account.loaded && proposal->author_user_id[0] != '\0' &&
                 strcmp(app->account.public_id, proposal->author_user_id) == 0;
    if(can_delete)
        h += ScaleUIPx(48);
    card = (Rectangle){(float)x, (float)y, (float)w, (float)h};
    DrawRectangleRounded(card, 0.08f, 10, WHITE);
    DrawRectangleRoundedLinesEx(card, 0.08f, 10, ScaleUIPx(1), GetThemeText());
    draw_text_font(font, fit_tail(font, proposal->title, body_font, w - ScaleUIPx(24)),
                   x + ScaleUIPx(12), y + ScaleUIPx(10), body_font, GetThemeText());
    if(proposal->description[0] != '\0')
        draw_wrapped_text(font, proposal->description, x + ScaleUIPx(12), y + ScaleUIPx(38),
                          w - ScaleUIPx(24), small_font, line_h, GetThemeText());
    if(can_delete) {
        draw_button(app, font, x + ScaleUIPx(12), y + h - ScaleUIPx(42),
                    w - ScaleUIPx(24), ScaleUIPx(34), "Delete proposal", 0,
                    UKU_FOCUS_PROPOSAL_DELETE_BASE + index, &delete_clicked);
        if(delete_clicked) {
            app->proposal_submit_failed = !lyra_delete_proposal(app, app->server_url, proposal->id);
            app->proposal_submit_ok = !app->proposal_submit_failed;
        }
    }
    return y + h + ScaleUIPx(12);
}

static int
draw_score_row(UkuApp *app, Font font, UkuProposal *proposal, int index,
               int x, int y, int w, int body_font, int small_font)
{
    int btn = ScaleUIPx(34);
    int score_w = ScaleUIPx(48);
    int h = ScaleUIPx(78);
    int minus_clicked = 0;
    int plus_clicked = 0;
    char score[16];
    Rectangle card = {(float)x, (float)y, (float)w, (float)h};

    DrawRectangleRounded(card, 0.08f, 10, WHITE);
    DrawRectangleRoundedLinesEx(card, 0.08f, 10, ScaleUIPx(1), GetThemeText());
    draw_text_font(font, fit_tail(font, proposal->title, body_font,
                                  w - btn * 2 - score_w - ScaleUIPx(52)),
                   x + ScaleUIPx(12), y + ScaleUIPx(12), body_font, GetThemeText());
    if(proposal->description[0] != '\0')
        draw_text_font(font, fit_tail(font, proposal->description, small_font,
                                      w - btn * 2 - score_w - ScaleUIPx(52)),
                       x + ScaleUIPx(12), y + ScaleUIPx(42), small_font, GetThemeText());

    draw_button(app, font, x + w - btn * 2 - score_w - ScaleUIPx(20), y + ScaleUIPx(22),
                btn, btn, "-", 0, UKU_FOCUS_SCORE_BASE + index * 2, &minus_clicked);
    snprintf(score, sizeof(score), "%d", proposal->score);
    DrawRectangleRounded((Rectangle){(float)(x + w - btn - score_w - ScaleUIPx(16)),
                                     (float)(y + ScaleUIPx(22)), (float)score_w, (float)btn},
                         0.08f, 10, GetThemeSurface());
    DrawRectangleRoundedLinesEx((Rectangle){(float)(x + w - btn - score_w - ScaleUIPx(16)),
                                            (float)(y + ScaleUIPx(22)), (float)score_w, (float)btn},
                                0.08f, 10, ScaleUIPx(1), GetThemeText());
    draw_centered_text(font, score, x + w - btn - score_w / 2 - ScaleUIPx(16),
                       GetUIControlTextY(score, y + ScaleUIPx(22), btn, body_font),
                       body_font, GetThemeText());
    draw_button(app, font, x + w - btn - ScaleUIPx(12), y + ScaleUIPx(22),
                btn, btn, "+", 0, UKU_FOCUS_SCORE_BASE + index * 2 + 1, &plus_clicked);
    if(minus_clicked)
        proposal->score = clampi(proposal->score - 1, -3, 3);
    if(plus_clicked)
        proposal->score = clampi(proposal->score + 1, -3, 3);
    return y + h + ScaleUIPx(12);
}

static int
draw_result_row(UkuApp *app, Font font, const UkuProposal *proposal,
                int rank, int x, int y, int w, int body_font, int small_font)
{
    char meta[128];
    char title[220];
    char resistance[80];
    int h = ScaleUIPx(82);
    Rectangle card = {(float)x, (float)y, (float)w, (float)h};

    DrawRectangleRounded(card, 0.08f, 10, WHITE);
    DrawRectangleRoundedLinesEx(card, 0.08f, 10, ScaleUIPx(1), GetThemeText());
    snprintf(title, sizeof(title), "%d. %s%s", rank, proposal->title, rank == 1 ? " - leading" : "");
    draw_text_font(font, fit_tail(font, title, body_font, w - ScaleUIPx(120)),
                   x + ScaleUIPx(12), y + ScaleUIPx(10), body_font, GetThemeText());
    snprintf(meta, sizeof(meta), "total %d | support %d | resistance %d | %d votes",
             proposal->total, proposal->positive_total, proposal->negative_total,
             proposal->vote_count);
    draw_text_font(font, fit_tail(font, meta, small_font, w - ScaleUIPx(24)),
                   x + ScaleUIPx(12), y + ScaleUIPx(42), small_font, GetThemeButton());
    if(proposal->vote_count > 0 && proposal->negative_total < 0 &&
       -proposal->negative_total >= proposal->positive_total) {
        snprintf(resistance, sizeof(resistance), "resistance at least equals support");
        draw_text_font(font, fit_tail(font, resistance, small_font, w - ScaleUIPx(24)),
                       x + ScaleUIPx(12), y + ScaleUIPx(62), small_font, GetThemeText());
    }
    (void)app;
    return y + h + ScaleUIPx(12);
}

static void
sort_result_indices(const UkuApp *app, int *indices, int count)
{
    for(int i = 0; i < count; i++)
        indices[i] = i;
    for(int i = 1; i < count; i++) {
        int key = indices[i];
        int j = i - 1;
        while(j >= 0) {
            const UkuProposal *a = &app->proposals[indices[j]];
            const UkuProposal *b = &app->proposals[key];
            if(a->total > b->total ||
               (a->total == b->total && a->negative_total > b->negative_total) ||
               (a->total == b->total && a->negative_total == b->negative_total &&
                a->positive_total >= b->positive_total))
                break;
            indices[j + 1] = indices[j];
            j--;
        }
        indices[j + 1] = key;
    }
}

static int
draw_participant_list(UkuApp *app, Font font, int x, int y, int w, int body_font, int small_font)
{
    int line_h = small_font + ScaleUIPx(7);
    char title[96];

    snprintf(title, sizeof(title), "Participants (%d)", app->vote_count);
    draw_text_font(font, title, x, y, body_font, GetThemeText());
    y += body_font + ScaleUIPx(10);
    if(app->vote_count <= 0) {
        y = draw_wrapped_text(font, "No votes submitted yet.", x, y, w, small_font,
                              line_h, GetThemeButton());
        return y + ScaleUIPx(12);
    }
    for(int i = 0; i < app->vote_count; i++) {
        const UkuVoteInfo *vote = &app->votes[i];
        char row[180];
        const char *name = vote->display_name[0] != '\0' ? vote->display_name : vote->voter_user_id;

        if(name == NULL || name[0] == '\0')
            name = "anonymous";
        snprintf(row, sizeof(row), "%s%s", name,
                 app->account.loaded && strcmp(vote->voter_user_id, app->account.public_id) == 0 ? " (you)" : "");
        draw_text_font(font, fit_tail(font, row, small_font, w - ScaleUIPx(24)),
                       x + ScaleUIPx(12), y, small_font, GetThemeText());
        y += line_h;
        if(vote->reason[0] != '\0') {
            char reason[460];
            snprintf(reason, sizeof(reason), "Reason: %s", vote->reason);
            y = draw_wrapped_text(font, reason, x + ScaleUIPx(12), y, w - ScaleUIPx(24),
                                  small_font, line_h, GetThemeButton());
        }
    }
    return y + ScaleUIPx(12);
}

static void
draw_collect(UkuApp *app, const UkuText *text, int view_w, int view_h)
{
    int side = GetUIPageSidePadding();
    int content_x;
    int content_w;
    int top_h = ScaleUIPx(58);
    int viewport_y = top_h;
    int viewport_h = view_h - viewport_y;
    int body_font = ClampUIPx(16, 16, 20);
    int small_font = ClampUIPx(13, 13, 16);
    int line_h = body_font + ScaleUIPx(8);
    int y = viewport_y + ScaleUIPx(20) - app->collect_scroll;
    int back_clicked = 0;
    int copy_clicked = 0;
    int public_clicked = 0;
    int private_clicked = 0;
    int delete_process_clicked = 0;
    int export_clicked = 0;
    int submit_clicked = 0;
    int content_bottom;
    int content_h;
    int is_owner;
    int result_indices[UKU_MAX_PROPOSALS];
    Font font = app->font;
    UkuDecision *d = &app->decision;
    char timer[128];
    char governance_line[256];
    int proposal_total = duration_minutes(d->proposal_days, d->proposal_hours, d->proposal_minutes);
    int voting_total = duration_minutes(d->voting_days, d->voting_hours, d->voting_minutes);
    sqlite3_int64 now = (sqlite3_int64)time(NULL);
    UkuProcessPhase phase;

    app->collect_scroll = clampi(app->collect_scroll - (int)(GetMouseWheelMove() * ScaleUIPx(44)),
                                 0, app->collect_max_scroll);
    GetUICenteredColumn(680, side, &content_x, &content_w);
    lyra_fetch_process_detail(app, app->server_url, text);
    if(app->proposal_count <= 0)
        load_default_proposals(app, text);
    proposal_total = duration_minutes(d->proposal_days, d->proposal_hours, d->proposal_minutes);
    voting_total = duration_minutes(d->voting_days, d->voting_hours, d->voting_minutes);
    phase = process_phase(d->created_at, proposal_total, voting_total, now, NULL);
    is_owner = app->account.loaded && d->owner_user_id[0] != '\0' &&
               strcmp(app->account.public_id, d->owner_user_id) == 0;

    draw_top_bar(app, text->collect_title, 1, UKU_FOCUS_COLLECT_BACK, &back_clicked, 0, NULL, 0, NULL, 0, NULL, view_w);
    if(back_clicked) {
        app->screen = UKU_SCREEN_HOME;
        db_load_processes(app);
        ClearUIFocus();
    }

    BeginScissorMode(0, viewport_y, view_w, viewport_h);
    draw_text_font(font, d->topic, content_x, y, body_font, GetThemeText());
    y += body_font + ScaleUIPx(18);
    if(d->description[0] != '\0') {
        y = draw_wrapped_text(font, d->description, content_x, y, content_w, small_font,
                              line_h, GetThemeText());
        y += ScaleUIPx(14);
    }

    format_process_timer(timer, sizeof(timer), text, d->created_at, proposal_total, voting_total, now);
    DrawRectangleRounded((Rectangle){(float)content_x, (float)y, (float)content_w, (float)ScaleUIPx(44)}, 0.08f, 10,
                         (Color){255, 255, 255, 255});
    DrawRectangleRoundedLinesEx((Rectangle){(float)content_x, (float)y, (float)content_w, (float)ScaleUIPx(44)}, 0.08f, 10,
                                ScaleUIPx(1), GetThemeText());
    draw_text_font(font, fit_tail(font, timer, body_font, content_w - ScaleUIPx(24)),
                   content_x + ScaleUIPx(12), y + ScaleUIPx(12), body_font,
                   process_phase(d->created_at, proposal_total, voting_total, now, NULL) == UKU_PROCESS_RESULTS ? GetThemeText() : GetThemeButton());
    y += ScaleUIPx(62);

    snprintf(governance_line, sizeof(governance_line),
             "Method %s | Quorum %d%% | Vote reason %s",
             d->decision_type[0] != '\0' ? d->decision_type : "consent",
             d->quorum_percent,
             d->require_vote_reason ? "required" : "optional");
    y = draw_wrapped_text(font, governance_line, content_x, y, content_w, small_font,
                          small_font + ScaleUIPx(7), GetThemeButton());
    if(d->outcome[0] != '\0') {
        snprintf(governance_line, sizeof(governance_line), "Outcome: %s", d->outcome);
        y = draw_wrapped_text(font, governance_line, content_x, y, content_w, small_font,
                              small_font + ScaleUIPx(7), GetThemeText());
    }
    if(d->review_at[0] != '\0') {
        snprintf(governance_line, sizeof(governance_line), "Review: %s", d->review_at);
        y = draw_wrapped_text(font, governance_line, content_x, y, content_w, small_font,
                              small_font + ScaleUIPx(7), GetThemeText());
    }
    y += ScaleUIPx(14);

    if(d->remote_error) {
        y = draw_wrapped_text(font, "Saved locally, but Lyra upload failed. Check your connection and account.", content_x, y, content_w, small_font, line_h, GetThemeButton());
        y += ScaleUIPx(16);
    }
    if(app->process_detail_loading_failed) {
        y = draw_wrapped_text(font, "Could not refresh this process from Lyra. Showing local details.", content_x, y, content_w, small_font, line_h, GetThemeButton());
        y += ScaleUIPx(16);
    }

    draw_text_font(font, text->local_address_label, content_x, y, small_font, GetThemeButton());
    y += small_font + ScaleUIPx(8);
    DrawRectangleRounded((Rectangle){(float)content_x, (float)y, (float)content_w, (float)ScaleUIPx(46)}, 0.08f, 10, WHITE);
    DrawRectangleRoundedLinesEx((Rectangle){(float)content_x, (float)y, (float)content_w, (float)ScaleUIPx(46)}, 0.08f, 10,
                                ScaleUIPx(1), GetThemeText());
    draw_text_font(font, d->local_address, content_x + ScaleUIPx(12), y + ScaleUIPx(13), body_font, GetThemeText());
    draw_button(app, font, content_x, y + ScaleUIPx(54), content_w, ScaleUIPx(40),
                "Copy share address", 0, UKU_FOCUS_PUBLIC_ID_COPY, &copy_clicked);
    if(copy_clicked) {
        SetClipboardText(d->local_address);
        copy_text(app->process_status, sizeof(app->process_status),
                  "Share address copied.", strlen("Share address copied."));
    }
    y += ScaleUIPx(112);

    if(is_owner) {
        int half = (content_w - ScaleUIPx(10)) / 2;
        char owner_line[96];

        snprintf(owner_line, sizeof(owner_line), "Owner controls | %s",
                 d->visibility[0] != '\0' ? d->visibility : "public");
        draw_text_font(font, owner_line, content_x, y, small_font, GetThemeButton());
        y += small_font + ScaleUIPx(8);
        draw_button(app, font, content_x, y, half, ScaleUIPx(40), "Public", 0,
                    UKU_FOCUS_PROCESS_PUBLIC, &public_clicked);
        draw_button(app, font, content_x + half + ScaleUIPx(10), y, half, ScaleUIPx(40),
                    "Private", 0, UKU_FOCUS_PROCESS_PRIVATE, &private_clicked);
        if(public_clicked || private_clicked) {
            const char *next_visibility = public_clicked ? "public" : "unlisted";
            app->process_update_failed =
                !lyra_update_process_visibility(app, app->server_url, next_visibility);
            if(!app->process_update_failed) {
                copy_text(app->process_status, sizeof(app->process_status),
                          "Process visibility updated.", strlen("Process visibility updated."));
            }
        }
        y += ScaleUIPx(50);
        draw_button(app, font, content_x, y, content_w, ScaleUIPx(40), "Export decision packet", 0,
                    UKU_FOCUS_PROCESS_EXPORT, &export_clicked);
        if(export_clicked) {
            app->process_export_failed = !lyra_export_process(app, app->server_url);
            copy_text(app->process_status, sizeof(app->process_status),
                      app->process_export_failed ? "Could not export decision packet." : "Decision packet copied.",
                      strlen(app->process_export_failed ? "Could not export decision packet." : "Decision packet copied."));
        }
        y += ScaleUIPx(50);
        draw_button(app, font, content_x, y, content_w, ScaleUIPx(40), "Archive process", 0,
                    UKU_FOCUS_PROCESS_DELETE, &delete_process_clicked);
        if(delete_process_clicked) {
            app->process_update_failed = !lyra_delete_process(app, app->server_url);
            if(!app->process_update_failed) {
                app->remote_processes_loaded = 0;
                app->screen = UKU_SCREEN_HOME;
                ClearUIFocus();
                EndScissorMode();
                return;
            }
        }
        y += ScaleUIPx(54);
        if(app->process_update_failed) {
            y = draw_wrapped_text(font, "Could not update process visibility.", content_x, y,
                                  content_w, small_font, line_h, GetThemeButton());
            y += ScaleUIPx(12);
        }
    }

    if(phase == UKU_PROCESS_PROPOSAL) {
        draw_text_font(font, "Add proposal", content_x, y, body_font, GetThemeText());
        y += body_font + ScaleUIPx(12);
        if(!app->account.loaded) {
            y = draw_wrapped_text(font, "Create or import an account before adding proposals.", content_x, y,
                                  content_w, small_font, line_h, GetThemeButton());
            y += ScaleUIPx(14);
        } else {
            y = draw_text_field(app, font, "Title", "Proposal title",
                                app->proposal_title, sizeof(app->proposal_title),
                                UKU_FIELD_PROPOSAL_TITLE, UKU_FOCUS_PROPOSAL_TITLE,
                                content_x, y, content_w, ScaleUIPx(46));
            y = draw_text_field(app, font, "Description", "Optional details",
                                app->proposal_description, sizeof(app->proposal_description),
                                UKU_FIELD_PROPOSAL_DESCRIPTION, UKU_FOCUS_PROPOSAL_DESCRIPTION,
                                content_x, y, content_w, ScaleUIPx(82));
            draw_button(app, font, content_x, y, content_w, ScaleUIPx(46), "Submit proposal", 1,
                        UKU_FOCUS_PROPOSAL_SUBMIT, &submit_clicked);
            if(submit_clicked) {
                app->proposal_submit_ok = 0;
                app->proposal_submit_failed = !lyra_submit_proposal(app, app->server_url);
                app->proposal_submit_ok = !app->proposal_submit_failed;
            }
            y += ScaleUIPx(60);
        }
        if(app->proposal_submit_ok)
            y = draw_wrapped_text(font, "Proposal submitted.", content_x, y, content_w,
                                  small_font, line_h, GetThemeText());
        if(app->proposal_submit_failed)
            y = draw_wrapped_text(font, "Could not submit proposal.", content_x, y, content_w,
                                  small_font, line_h, GetThemeButton());
        y += ScaleUIPx(12);
    } else if(phase == UKU_PROCESS_VOTING) {
        draw_text_font(font, "Your ballot", content_x, y, body_font, GetThemeText());
        y += body_font + ScaleUIPx(12);
        if(!app->account.loaded) {
            y = draw_wrapped_text(font, "Create or import an account before voting.", content_x, y,
                                  content_w, small_font, line_h, GetThemeButton());
            y += ScaleUIPx(14);
        } else {
            for(int i = 0; i < app->proposal_count; i++)
                y = draw_score_row(app, font, &app->proposals[i], i, content_x, y,
                                   content_w, body_font, small_font);
            y = draw_text_field(app, font, "Reason", d->require_vote_reason ? "Required voting reason" : "Optional voting reason",
                                app->vote_reason, sizeof(app->vote_reason),
                                UKU_FIELD_VOTE_REASON, UKU_FOCUS_VOTE_REASON,
                                content_x, y, content_w, ScaleUIPx(82));
            draw_button(app, font, content_x, y, content_w, ScaleUIPx(46),
                        app->current_user_voted ? "Update vote" : "Submit vote", 1,
                        UKU_FOCUS_VOTE_SUBMIT, &submit_clicked);
            if(submit_clicked) {
                app->vote_submit_ok = 0;
                app->vote_submit_failed = !lyra_submit_vote(app, app->server_url);
                app->vote_submit_ok = !app->vote_submit_failed;
            }
            y += ScaleUIPx(60);
        }
        if(app->vote_submit_ok)
            y = draw_wrapped_text(font, "Vote submitted.", content_x, y, content_w,
                                  small_font, line_h, GetThemeText());
        if(app->vote_submit_failed)
            y = draw_wrapped_text(font, "Could not submit vote.", content_x, y, content_w,
                                  small_font, line_h, GetThemeButton());
        y += ScaleUIPx(12);
    } else {
        draw_text_font(font, "Results", content_x, y, body_font, GetThemeText());
        y += body_font + ScaleUIPx(12);
        sort_result_indices(app, result_indices, app->proposal_count);
        for(int i = 0; i < app->proposal_count; i++)
            y = draw_result_row(app, font, &app->proposals[result_indices[i]], i + 1,
                                content_x, y, content_w, body_font, small_font);
        y += ScaleUIPx(8);
    }

    y = draw_participant_list(app, font, content_x, y, content_w, body_font, small_font);

    draw_text_font(font, "Proposals", content_x, y, body_font, GetThemeText());
    y += body_font + ScaleUIPx(12);
    for(int i = 0; i < app->proposal_count; i++)
        y = draw_proposal_card(app, font, &app->proposals[i], i, content_x, y,
                               content_w, body_font, small_font);
    if(app->process_status[0] != '\0') {
        y += ScaleUIPx(4);
        y = draw_wrapped_text(font, app->process_status, content_x, y, content_w,
                              small_font, line_h, GetThemeText());
    }
    EndScissorMode();

    content_bottom = y + app->collect_scroll + ScaleUIPx(24);
    content_h = content_bottom - viewport_y;
    app->collect_max_scroll = UKU_MAX(0, content_h - viewport_h);
    app->collect_scroll = clampi(app->collect_scroll, 0, app->collect_max_scroll);
    draw_scrollbar(app, view_w - side - ScaleUIPx(8), viewport_y + ScaleUIPx(8),
                   viewport_h - ScaleUIPx(16), content_h, app->collect_max_scroll,
                   &app->collect_scroll, &app->collect_drag_scrollbar,
                   &app->collect_scroll_drag_offset);
}

static void
draw_public_id_modal(UkuApp *app, int view_w, int view_h)
{
    int panel_w = UKU_MIN(view_w - ScaleUIPx(32), ScaleUIPx(380));
    int panel_h = ScaleUIPx(274);
    int x = (view_w - panel_w) / 2;
    int y = (view_h - panel_h) / 2;
    int pad = ScaleUIPx(18);
    int body_font = ClampUIPx(15, 15, 19);
    int small_font = ClampUIPx(13, 13, 16);
    int line_h = body_font + ScaleUIPx(7);
    int copy_clicked = 0;
    int close_clicked = 0;
    int alias_clicked = 0;
    int content_y;
    int half_w = (panel_w - pad * 2 - ScaleUIPx(10)) / 2;
    Rectangle overlay = {0, 0, (float)view_w, (float)view_h};
    Rectangle panel = {(float)x, (float)y, (float)panel_w, (float)panel_h};
    Font font = app->font;

    if(!app->account_public_id_modal_open)
        return;

    DrawRectangleRec(overlay, (Color){0, 0, 0, 96});
    DrawRectangleRounded(panel, 0.08f, 12, GetThemeSurface());
    DrawRectangleRoundedLinesEx(panel, 0.08f, 12, ScaleUIPx(1), GetThemeText());
    draw_text_font(font, "Full Public ID", x + pad, y + pad, body_font, GetThemeText());
    content_y = y + pad + body_font + ScaleUIPx(14);
    content_y = draw_wrapped_text(font, "This is the full account ID used by the server.",
                                  x + pad, content_y, panel_w - pad * 2, small_font, line_h,
                                  GetThemeText());
    content_y += ScaleUIPx(10);
    content_y = draw_readonly_field(app, font, app->account.public_id,
                                    x + pad, content_y, panel_w - pad * 2, ScaleUIPx(46),
                                    UKU_FOCUS_ACCOUNT_ID, &copy_clicked);
    if(copy_clicked) {
        SetClipboardText(app->account.public_id);
        copy_text(app->account_status, sizeof(app->account_status),
                  "Public ID copied.", strlen("Public ID copied."));
    }
    content_y += ScaleUIPx(4);
    draw_button(app, font, x + pad, content_y, half_w, ScaleUIPx(40), "Copy", 1,
                UKU_FOCUS_PUBLIC_ID_COPY, &copy_clicked);
    draw_button(app, font, x + pad + half_w + ScaleUIPx(10), content_y, half_w, ScaleUIPx(40),
                "Close", 0, UKU_FOCUS_PUBLIC_ID_CLOSE, &close_clicked);
    if(copy_clicked) {
        SetClipboardText(app->account.public_id);
        copy_text(app->account_status, sizeof(app->account_status),
                  "Public ID copied.", strlen("Public ID copied."));
    }
    if(close_clicked)
        app->account_public_id_modal_open = 0;

    draw_button(app, font, x + pad, y + panel_h - pad - ScaleUIPx(40),
                panel_w - pad * 2, ScaleUIPx(40), "Alias", 0,
                UKU_FOCUS_PUBLIC_ID_ALIAS, &alias_clicked);
    if(alias_clicked)
        account_open_alias_modal(app);
}

static void
draw_process_type_modal(UkuApp *app, int view_w, int view_h)
{
    int count = (int)(sizeof(process_templates) / sizeof(process_templates[0]));
    int panel_w = UKU_MIN(view_w - ScaleUIPx(28), ScaleUIPx(520));
    int row_h = ScaleUIPx(82);
    int panel_h = ScaleUIPx(88) + row_h * count + ScaleUIPx(18);
    int max_h = view_h - ScaleUIPx(28);
    int x;
    int y;
    int pad = ScaleUIPx(18);
    int title_font = ClampUIPx(18, 18, 22);
    int item_font = ClampUIPx(16, 16, 20);
    int body_font = ClampUIPx(13, 13, 16);
    int close_clicked = 0;
    Rectangle overlay = {0, 0, (float)view_w, (float)view_h};
    Rectangle panel;
    Font font = app->font;
    Vector2 mouse = GetMousePosition();

    if(!app->process_type_modal_open)
        return;

    if(panel_h > max_h)
        panel_h = max_h;
    x = (view_w - panel_w) / 2;
    y = (view_h - panel_h) / 2;
    panel = (Rectangle){(float)x, (float)y, (float)panel_w, (float)panel_h};

    DrawRectangleRec(overlay, (Color){0, 0, 0, 96});
    DrawRectangleRounded(panel, 0.08f, 12, GetThemeSurface());
    DrawRectangleRoundedLinesEx(panel, 0.08f, 12, ScaleUIPx(1), GetThemeText());

    draw_text_font(font, "Choose process type", x + pad, y + pad, title_font, GetThemeText());
    draw_button(app, font, x + panel_w - pad - ScaleUIPx(92), y + ScaleUIPx(12),
                ScaleUIPx(92), ScaleUIPx(36), "Close", 0,
                UKU_FOCUS_PROCESS_TYPE_CLOSE, &close_clicked);
    if(close_clicked)
        app->process_type_modal_open = 0;

    y += ScaleUIPx(62);
    BeginScissorMode(x, y, panel_w, panel_h - ScaleUIPx(76));
    for(int i = 0; i < count; i++) {
        const UkuProcessTemplate *tmpl = &process_templates[i];
        int row_y = y + i * row_h;
        Rectangle row = {(float)(x + pad), (float)row_y,
                         (float)(panel_w - pad * 2), (float)(row_h - ScaleUIPx(8))};
        int selected = process_template_selected(&app->decision, tmpl);
        int hovered = CheckCollisionPointRec(mouse, row);
        int focused = RegisterUIFocus(UKU_FOCUS_PROCESS_TYPE_BASE + 1 + i, row);

        if(hovered)
            app->cursor_clickable = 1;
        DrawRectangleRounded(row, 0.08f, 10,
                             selected ? (Color){232, 241, 247, 255} : GetThemeSurface());
        DrawRectangleRoundedLinesEx(row, 0.08f, 10, ScaleUIPx(selected || focused ? 2 : 1),
                                    selected || focused ? GetThemeButton() : GetThemeText());
        if(focused)
            DrawUIFocus(row);
        if(selected)
            DrawRectangle((int)row.x + ScaleUIPx(7), (int)row.y + ScaleUIPx(12),
                          ScaleUIPx(4), (int)row.height - ScaleUIPx(24), GetThemeButton());

        draw_text_font(font, fit_tail(font, tmpl->title, item_font,
                                      (int)row.width - ScaleUIPx(30)),
                       (int)row.x + ScaleUIPx(18), (int)row.y + ScaleUIPx(11),
                       item_font, GetThemeText());
        draw_text_font(font, fit_tail(font, tmpl->description, body_font,
                                      (int)row.width - ScaleUIPx(30)),
                       (int)row.x + ScaleUIPx(18), (int)row.y + ScaleUIPx(42),
                       body_font, GetThemeButton());

        if((hovered && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) ||
           IsUIFocusActivatePressed(UKU_FOCUS_PROCESS_TYPE_BASE + 1 + i)) {
            apply_process_template(&app->decision, tmpl);
            app->process_type_modal_open = 0;
            app->active_field = UKU_FIELD_NONE;
        }
    }
    EndScissorMode();
}

static void
draw_alias_modal(UkuApp *app, int view_w, int view_h)
{
    int panel_w = UKU_MIN(view_w - ScaleUIPx(32), ScaleUIPx(360));
    int panel_h = ScaleUIPx(248);
    int x = (view_w - panel_w) / 2;
    int y = (view_h - panel_h) / 2;
    int pad = ScaleUIPx(18);
    int body_font = ClampUIPx(15, 15, 19);
    int small_font = ClampUIPx(13, 13, 16);
    int line_h = body_font + ScaleUIPx(7);
    int close_clicked = 0;
    int save_clicked = 0;
    int input_x;
    int input_w;
    int content_y;
    Rectangle overlay = {0, 0, (float)view_w, (float)view_h};
    Rectangle panel = {(float)x, (float)y, (float)panel_w, (float)panel_h};
    Font font = app->font;

    if(!app->account_alias_modal_open)
        return;

    DrawRectangleRec(overlay, (Color){0, 0, 0, 96});
    DrawRectangleRounded(panel, 0.08f, 12, GetThemeSurface());
    DrawRectangleRoundedLinesEx(panel, 0.08f, 12, ScaleUIPx(1), GetThemeText());
    draw_text_font(font, "Account alias", x + pad, y + pad, body_font, GetThemeText());
    content_y = y + pad + body_font + ScaleUIPx(14);
    content_y = draw_wrapped_text(font, "Choose a short alias for this account on the current server.",
                                  x + pad, content_y, panel_w - pad * 2, small_font, line_h,
                                  GetThemeText());
    content_y += ScaleUIPx(8);
    draw_text_font(font, "@", x + pad, content_y + ScaleUIPx(30), body_font, GetThemeText());
    input_x = x + pad + ScaleUIPx(24);
    input_w = panel_w - pad * 2 - ScaleUIPx(24);
    draw_text_field(app, font, "Alias", "name",
                    app->alias_input, sizeof(app->alias_input), UKU_FIELD_ALIAS,
                    UKU_FOCUS_ALIAS_FIELD, input_x, content_y, input_w, ScaleUIPx(42));
    alias_normalize(app->alias_input);
    content_y += ScaleUIPx(74);
    draw_text_font(font, "4-32 letters, numbers, or underscore.", x + pad, content_y,
                   small_font, GetThemeButton());
    content_y += ScaleUIPx(28);
    draw_button(app, font, x + pad, content_y, (panel_w - pad * 2 - ScaleUIPx(10)) / 2,
                ScaleUIPx(40), "Close", 0, UKU_FOCUS_ALIAS_CLOSE, &close_clicked);
    draw_button(app, font, x + pad + (panel_w - pad * 2 + ScaleUIPx(10)) / 2, content_y,
                (panel_w - pad * 2 - ScaleUIPx(10)) / 2, ScaleUIPx(40), "Save", 1,
                UKU_FOCUS_ALIAS_SAVE, &save_clicked);

    if(close_clicked) {
        app->account_alias_modal_open = 0;
        app->active_field = UKU_FIELD_NONE;
    } else if(save_clicked) {
        alias_normalize(app->alias_input);
        if(alias_valid(app->alias_input) &&
           lyra_register_alias(app, app->server_url, app->alias_input)) {
            copy_text(app->account_status, sizeof(app->account_status),
                      "Alias saved.", strlen("Alias saved."));
            app->account_alias_modal_open = 0;
            app->active_field = UKU_FIELD_NONE;
        } else {
            copy_text(app->account_status, sizeof(app->account_status),
                      "Could not save alias.", strlen("Could not save alias."));
        }
    }
}

static void
draw_account(UkuApp *app, const UkuText *text, int view_w, int view_h)
{
    int side = GetUIPageSidePadding();
    int content_x;
    int content_w;
    int top_h = ScaleUIPx(58);
    int y = top_h + ScaleUIPx(24);
    int body_font = ClampUIPx(15, 15, 19);
    int small_font = ClampUIPx(13, 13, 16);
    int line_h = body_font + ScaleUIPx(8);
    int back_clicked = 0;
    int create_clicked = 0;
    int import_clicked = 0;
    int save_server_clicked = 0;
    int export_clicked = 0;
    int account_id_clicked = 0;
    char alias[40];
    char display_id[96];
    Font font = app->font;

    GetUICenteredColumn(680, side, &content_x, &content_w);
    draw_top_bar(app, "Account", 1, UKU_FOCUS_MANUAL_BACK, &back_clicked, 0, NULL, 0, NULL, 0, NULL, view_w);
    if(back_clicked) {
        app->screen = UKU_SCREEN_HOME;
        ClearUIFocus();
    }

    y = draw_text_field(app, font, "Lyra server", "https://api.waozi.xyz",
                        app->server_url, sizeof(app->server_url), UKU_FIELD_SERVER_URL,
                        UKU_FOCUS_TOPIC, content_x, y, content_w, ScaleUIPx(46));
    draw_button(app, font, content_x, y, content_w, ScaleUIPx(42), "Save server URL", 0,
                UKU_FOCUS_DESCRIPTION, &save_server_clicked);
    if(save_server_clicked)
        sync_server_save(app);
    y += ScaleUIPx(54);
    if(app->server_url_error) {
        y = draw_wrapped_text(font, "Use HTTPS for remote servers, or localhost/127.0.0.1/10.0.2.2 for HTTP development.", content_x, y, content_w, small_font, line_h, GetThemeButton());
        y += ScaleUIPx(12);
    }

    if(app->account.loaded) {
        account_refresh_alias_once(app);
        setting_load_text(app, UKU_SYNC_ACCOUNT_ALIAS_KEY, "", alias, sizeof(alias));
        if(alias[0] != '\0')
            snprintf(display_id, sizeof(display_id), "@%s", alias);
        else
            compact_public_id(app->account.public_id, display_id, sizeof(display_id));
        draw_text_font(font, "Public ID", content_x, y, small_font, GetThemeButton());
        y += small_font + ScaleUIPx(8);
        y = draw_readonly_field(app, font, display_id, content_x, y, content_w, ScaleUIPx(46),
                                UKU_FOCUS_ACCOUNT_ID, &account_id_clicked);
        if(account_id_clicked)
            account_open_public_id_modal(app);
        y += ScaleUIPx(6);
        draw_button(app, font, content_x, y, content_w, ScaleUIPx(46), "Export account.key", 0,
                    UKU_FOCUS_SETTINGS, &export_clicked);
        if(export_clicked)
            account_start_export_dialog(app);
        y += ScaleUIPx(58);
    } else {
        if(IsLyraAccountAvailable()) {
            y = draw_wrapped_text(font, "Create an account or import an account key to start processes, add proposals, or vote.", content_x, y, content_w, body_font, line_h, GetThemeText());
            y += ScaleUIPx(18);
            draw_button(app, font, content_x, y, content_w, ScaleUIPx(46), "Create account", 1,
                        UKU_FOCUS_CREATE_SUBMIT, &create_clicked);
            y += ScaleUIPx(58);
            draw_button(app, font, content_x, y, content_w, ScaleUIPx(46), "Import account.key", 0,
                        UKU_FOCUS_SETTINGS, &import_clicked);
            y += ScaleUIPx(58);
            if(create_clicked)
                account_create(app);
            if(import_clicked)
                account_start_import_dialog(app);
        } else {
            y = draw_wrapped_text(font, "This build does not include liboqs, so account creation and signing are unavailable.", content_x, y, content_w, body_font, line_h, GetThemeButton());
        }
    }
    if(app->account_status[0] != '\0')
        draw_wrapped_text(font, app->account_status, content_x, y, content_w, small_font, line_h,
                          app->account.import_failed ? GetThemeButton() : GetThemeText());

    (void)text;
    (void)view_h;
}

static void
draw_manual(UkuApp *app, const UkuText *text, int view_w, int view_h)
{
    int side = GetUIPageSidePadding();
    int content_x;
    int content_w;
    int top_h = ScaleUIPx(58);
    int viewport_y = top_h;
    int viewport_h = view_h - viewport_y;
    int body_font = ClampUIPx(15, 15, 19);
    int line_h = body_font + ScaleUIPx(7);
    int y = viewport_y + ScaleUIPx(24) - app->manual_scroll;
    int back_clicked = 0;
    int content_bottom;
    int content_h;
    Font font = app->font;

    app->manual_scroll = clampi(app->manual_scroll - (int)(GetMouseWheelMove() * ScaleUIPx(44)),
                                0, app->manual_max_scroll);
    GetUICenteredColumn(760, side, &content_x, &content_w);

    draw_top_bar(app, text->manual_title, 1, UKU_FOCUS_MANUAL_BACK, &back_clicked, 0, NULL, 0, NULL, 0, NULL, view_w);
    if(back_clicked) {
        app->screen = UKU_SCREEN_HOME;
        ClearUIFocus();
    }

    BeginScissorMode(0, viewport_y, view_w, viewport_h);
    y = draw_wrapped_text(font, text->manual_body, content_x, y, content_w, body_font, line_h, GetThemeText());
    EndScissorMode();

    content_bottom = y + app->manual_scroll + ScaleUIPx(24);
    content_h = content_bottom - viewport_y;
    app->manual_max_scroll = UKU_MAX(0, content_h - viewport_h);
    app->manual_scroll = clampi(app->manual_scroll, 0, app->manual_max_scroll);
    draw_scrollbar(app, view_w - side - ScaleUIPx(8), viewport_y + ScaleUIPx(8),
                   viewport_h - ScaleUIPx(16), content_h, app->manual_max_scroll,
                   &app->manual_scroll, &app->manual_drag_scrollbar, &app->manual_scroll_drag_offset);
}

int
main(void)
{
    UkuApp app = {0};
    UkuText text = {0};
    int window_w = 520;
    int window_h = 760;

    load_text_file(&text, LOCALE_TEXT_PATH);
    init_decision(&app, &text);
    db_init(&app);
    setting_load_text(&app, UKU_SYNC_SERVER_URL_KEY, UKU_SYNC_SERVER_URL_DEFAULT,
                      app.server_url, sizeof(app.server_url));
    {
        char normalized_url[sizeof(app.server_url)];
        if(sync_url_normalize(app.server_url, normalized_url, sizeof(normalized_url)))
            snprintf(app.server_url, sizeof(app.server_url), "%s", normalized_url);
        else
            snprintf(app.server_url, sizeof(app.server_url), "%s", UKU_SYNC_SERVER_URL_DEFAULT);
    }
    account_load(&app);
#if !defined(PLATFORM_WEB)
    InitFileDialog(&app.account_import_dialog);
    InitFileDialog(&app.account_export_dialog);
#else
    GetWebViewportSize(window_w, window_h, &window_w, &window_h);
#endif

#if defined(PLATFORM_WEB)
    SetConfigFlags(GetWebWindowFlags());
#else
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
#endif
    InitWindow(window_w, window_h, text.app_title);
    SetTargetFPS(60);
    InitUIDPI();
    SetCurrentTheme(THEME_SUNSET, 0);
    app_load_font(&app);
    load_icons_once(&app);

    while(!WindowShouldClose()) {
#if defined(PLATFORM_WEB)
        SyncWebWindowSize();
#endif
        int view_w = GetScreenWidth();
        int view_h = GetScreenHeight();

        UpdateUIDPI(view_w, view_h);
        SetUIScale(ui_dpi_state.ui_scale_clamped);
        SetUIViewSize(view_w, view_h);
        InitUI(view_w, view_h, GetUIScale());
        SetUIFrame((Camera2D){0});
        SetUICursorClickable(&app.cursor_clickable);
        SetUIColors(GetThemeText(), GetThemeBackground(), GetThemeSurface(), GetThemeCircle(), GetThemeButton(), GetThemeButtonHover(), GetThemeIcon());

        app.cursor_clickable = 0;

        BeginDrawing();
        ClearBackground(GetThemeBackground());
        BeginUIFocus();
        SetUIFocusTextInputActive(app.active_field != UKU_FIELD_NONE);
        if(app.screen == UKU_SCREEN_HOME)
            draw_home(&app, &text, view_w, view_h);
        else if(app.screen == UKU_SCREEN_CREATE)
            draw_create_placeholder(&app, &text, view_w, view_h);
        else if(app.screen == UKU_SCREEN_COLLECT)
            draw_collect(&app, &text, view_w, view_h);
        else if(app.screen == UKU_SCREEN_ACCOUNT)
            draw_account(&app, &text, view_w, view_h);
        else
            draw_manual(&app, &text, view_w, view_h);
        draw_process_type_modal(&app, view_w, view_h);
        draw_public_id_modal(&app, view_w, view_h);
        draw_alias_modal(&app, view_w, view_h);
        EndUIFocus();
        EndDrawing();

        SetMouseCursor(app.cursor_clickable ? MOUSE_CURSOR_POINTING_HAND : MOUSE_CURSOR_DEFAULT);
    }

    app_unload_font(&app);
#if !defined(PLATFORM_WEB)
    CloseFileDialog(&app.account_import_dialog);
    CloseFileDialog(&app.account_export_dialog);
    if(app.db != NULL)
        sqlite3_close(app.db);
#endif
    CloseWindow();
    return 0;
}
