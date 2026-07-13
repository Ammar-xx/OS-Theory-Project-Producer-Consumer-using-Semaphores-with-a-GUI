#include "dashboard.h"
#include <ncurses.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

#define LOG_SIZE 200
#define LINE_LEN 128

static WINDOW *win_buffer  = NULL;
static WINDOW *win_stats   = NULL;
static WINDOW *win_log     = NULL;
static WINDOW *win_threads = NULL;
static WINDOW *win_status  = NULL;

static buffer_t *g_buf = NULL;
static int g_n_p = 0, g_n_c = 0, g_total = 0;
static int g_produced = 0, g_consumed = 0;
static int *g_p_counts = NULL;
static int *g_c_counts = NULL;

/* circular activity log */
static char g_log[LOG_SIZE][LINE_LEN];
static int  g_log_head  = 0;
static int  g_log_count = 0;

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_t g_refresh_tid;
static volatile int g_running = 0;
static long g_start_us = 0;

static long now_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000L + tv.tv_usec;
}

/* caller must hold g_lock */
static void log_internal(const char *line) {
    strncpy(g_log[g_log_head], line, LINE_LEN - 1);
    g_log[g_log_head][LINE_LEN - 1] = '\0';
    g_log_head = (g_log_head + 1) % LOG_SIZE;
    if (g_log_count < LOG_SIZE) g_log_count++;
}

static void render_buffer_panel(void) {
    werase(win_buffer);
    box(win_buffer, 0, 0);
    wattron(win_buffer, COLOR_PAIR(3) | A_BOLD);
    mvwprintw(win_buffer, 0, 2, " BUFFER STATE ");
    wattroff(win_buffer, COLOR_PAIR(3) | A_BOLD);

    int empty_v, full_v;
    sem_getvalue(&g_buf->empty, &empty_v);
    sem_getvalue(&g_buf->full,  &full_v);
    int count = full_v;
    int size  = g_buf->size;

    mvwprintw(win_buffer, 2, 2, "Capacity:  %d slots", size);
    mvwprintw(win_buffer, 3, 2, "Filled:    %d / %d", count, size);

    /* slot visualization */
    mvwprintw(win_buffer, 5, 2, "Slots: [");
    for (int i = 0; i < size; i++) {
        if (i < count) {
            wattron(win_buffer, COLOR_PAIR(1) | A_BOLD);
            waddch(win_buffer, '#');
            wattroff(win_buffer, COLOR_PAIR(1) | A_BOLD);
        } else {
            wattron(win_buffer, COLOR_PAIR(2));
            waddch(win_buffer, '.');
            wattroff(win_buffer, COLOR_PAIR(2));
        }
    }
    wprintw(win_buffer, "]");

    /* fill bar */
    int bar_w = 40;
    int filled_chars = (size > 0) ? (count * bar_w) / size : 0;
    mvwprintw(win_buffer, 7, 2, "Fill:  [");
    wattron(win_buffer, COLOR_PAIR(1));
    for (int i = 0; i < filled_chars; i++) waddch(win_buffer, '=');
    wattroff(win_buffer, COLOR_PAIR(1));
    for (int i = filled_chars; i < bar_w; i++) waddch(win_buffer, ' ');
    int pct = (size > 0) ? (count * 100) / size : 0;
    wprintw(win_buffer, "] %3d%%", pct);

    wnoutrefresh(win_buffer);
}

static void render_stats_panel(void) {
    werase(win_stats);
    box(win_stats, 0, 0);
    wattron(win_stats, COLOR_PAIR(3) | A_BOLD);
    mvwprintw(win_stats, 0, 2, " STATISTICS ");
    wattroff(win_stats, COLOR_PAIR(3) | A_BOLD);

    mvwprintw(win_stats, 2, 2, "Producers: %d", g_n_p);
    mvwprintw(win_stats, 3, 2, "Consumers: %d", g_n_c);
    mvwprintw(win_stats, 4, 2, "Buf size:  %d", g_buf->size);

    wattron(win_stats, COLOR_PAIR(4));
    mvwprintw(win_stats, 6, 2, "Produced:  %d / %d", g_produced, g_total);
    wattroff(win_stats, COLOR_PAIR(4));
    wattron(win_stats, COLOR_PAIR(5));
    mvwprintw(win_stats, 7, 2, "Consumed:  %d / %d", g_consumed, g_total);
    wattroff(win_stats, COLOR_PAIR(5));

    double elapsed = (now_us() - g_start_us) / 1000000.0;
    mvwprintw(win_stats, 9,  2, "Elapsed:   %.2fs", elapsed);
    if (elapsed > 0)
        mvwprintw(win_stats, 10, 2, "Rate:      %.1f/s", g_consumed / elapsed);

    wnoutrefresh(win_stats);
}

static void render_log_panel(void) {
    werase(win_log);
    box(win_log, 0, 0);
    wattron(win_log, COLOR_PAIR(3) | A_BOLD);
    mvwprintw(win_log, 0, 2, " ACTIVITY LOG ");
    wattroff(win_log, COLOR_PAIR(3) | A_BOLD);

    int h, w;
    getmaxyx(win_log, h, w);
    int max_lines = h - 2;
    int n_show = (g_log_count < max_lines) ? g_log_count : max_lines;

    for (int i = 0; i < n_show; i++) {
        int idx = (g_log_head - n_show + i + LOG_SIZE) % LOG_SIZE;
        const char *line = g_log[idx];

        /* color by event type */
        int pair = 0;
        if (strstr(line, "produced"))      pair = 4;
        else if (strstr(line, "consumed")) pair = 5;
        else if (strstr(line, "--"))       pair = 3;

        if (pair) wattron(win_log, COLOR_PAIR(pair));
        mvwprintw(win_log, 1 + i, 2, "%.*s", w - 4, line);
        if (pair) wattroff(win_log, COLOR_PAIR(pair));
    }
    wnoutrefresh(win_log);
}

static void render_threads_panel(void) {
    werase(win_threads);
    box(win_threads, 0, 0);
    wattron(win_threads, COLOR_PAIR(3) | A_BOLD);
    mvwprintw(win_threads, 0, 2, " PER-THREAD ");
    wattroff(win_threads, COLOR_PAIR(3) | A_BOLD);

    int h, w;
    getmaxyx(win_threads, h, w);
    (void)w;
    int row = 1;

    for (int i = 0; i < g_n_p && row < h - 1; i++) {
        wattron(win_threads, COLOR_PAIR(4));
        mvwprintw(win_threads, row++, 2, "P%-2d %4d items", i, g_p_counts[i]);
        wattroff(win_threads, COLOR_PAIR(4));
    }
    if (row < h - 1) row++;
    for (int i = 0; i < g_n_c && row < h - 1; i++) {
        wattron(win_threads, COLOR_PAIR(5));
        mvwprintw(win_threads, row++, 2, "C%-2d %4d items", i, g_c_counts[i]);
        wattroff(win_threads, COLOR_PAIR(5));
    }
    wnoutrefresh(win_threads);
}

static void render_status_bar(void) {
    werase(win_status);
    box(win_status, 0, 0);
    int w;
    w = getmaxx(win_status);
    (void)w;

    mvwprintw(win_status, 1, 2,
              "Items consumed: %d / %d   |   Press 'q' to quit (after demo completes)",
              g_consumed, g_total);
    wnoutrefresh(win_status);
}

static void render_all(void) {
    pthread_mutex_lock(&g_lock);
    render_buffer_panel();
    render_stats_panel();
    render_log_panel();
    render_threads_panel();
    render_status_bar();
    doupdate();
    pthread_mutex_unlock(&g_lock);
}

static void *refresh_thread_fn(void *arg) {
    (void)arg;
    while (g_running) {
        render_all();
        usleep(50000);   /* ~20fps */
    }
    return NULL;
}

int dash_init(buffer_t *buf, int n_producers, int n_consumers, int total_items) {
    g_buf = buf;
    g_n_p = n_producers;
    g_n_c = n_consumers;
    g_total = total_items;
    g_produced = 0;
    g_consumed = 0;
    g_log_head = 0;
    g_log_count = 0;
    g_start_us = now_us();

    g_p_counts = calloc(n_producers, sizeof(int));
    g_c_counts = calloc(n_consumers, sizeof(int));
    if (!g_p_counts || !g_c_counts) return -1;

    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);

    if (!has_colors()) {
        endwin();
        fprintf(stderr, "Terminal does not support colors\n");
        return -1;
    }
    start_color();
    init_pair(1, COLOR_GREEN,   COLOR_BLACK);
    init_pair(2, COLOR_RED,     COLOR_BLACK);
    init_pair(3, COLOR_CYAN,    COLOR_BLACK);
    init_pair(4, COLOR_YELLOW,  COLOR_BLACK);
    init_pair(5, COLOR_MAGENTA, COLOR_BLACK);

    int term_h, term_w;
    getmaxyx(stdscr, term_h, term_w);

    if (term_h < 20 || term_w < 70) {
        endwin();
        fprintf(stderr, "Terminal too small (need at least 70x20, got %dx%d)\n",
                term_w, term_h);
        return -1;
    }

    /* title bar */
    attron(COLOR_PAIR(3) | A_BOLD);
    const char *title = "*** Producer-Consumer Live Dashboard ***";
    mvprintw(0, (term_w - (int)strlen(title)) / 2, "%s", title);
    attroff(COLOR_PAIR(3) | A_BOLD);
    refresh();

    int top_h    = (term_h - 4) / 2;
    int bot_h    = term_h - 4 - top_h;
    int left_w   = (term_w * 2) / 3;
    int right_w  = term_w - left_w;

    win_buffer  = newwin(top_h, left_w,  1, 0);
    win_stats   = newwin(top_h, right_w, 1, left_w);
    win_log     = newwin(bot_h, left_w,  1 + top_h, 0);
    win_threads = newwin(bot_h, right_w, 1 + top_h, left_w);
    win_status  = newwin(3, term_w, term_h - 3, 0);

    g_running = 1;
    if (pthread_create(&g_refresh_tid, NULL, refresh_thread_fn, NULL) != 0) {
        dash_close();
        return -1;
    }
    return 0;
}

void dash_close(void) {
    if (g_running) {
        g_running = 0;
        pthread_join(g_refresh_tid, NULL);
    }
    if (win_buffer)  delwin(win_buffer);
    if (win_stats)   delwin(win_stats);
    if (win_log)     delwin(win_log);
    if (win_threads) delwin(win_threads);
    if (win_status)  delwin(win_status);
    win_buffer = win_stats = win_log = win_threads = win_status = NULL;

    endwin();

    free(g_p_counts);
    free(g_c_counts);
    g_p_counts = g_c_counts = NULL;
}

void dash_log_produce(int p_id, int item_id) {
    char line[LINE_LEN];
    double e = (now_us() - g_start_us) / 1000000.0;
    snprintf(line, sizeof(line), "[%6.3fs] P%d produced item %d", e, p_id, item_id);

    pthread_mutex_lock(&g_lock);
    log_internal(line);
    g_produced++;
    if (p_id >= 0 && p_id < g_n_p) g_p_counts[p_id]++;
    pthread_mutex_unlock(&g_lock);
}

void dash_log_consume(int c_id, int item_id, int from_p) {
    char line[LINE_LEN];
    double e = (now_us() - g_start_us) / 1000000.0;
    snprintf(line, sizeof(line), "[%6.3fs] C%d consumed item %d from P%d",
             e, c_id, item_id, from_p);

    pthread_mutex_lock(&g_lock);
    log_internal(line);
    g_consumed++;
    if (c_id >= 0 && c_id < g_n_c) g_c_counts[c_id]++;
    pthread_mutex_unlock(&g_lock);
}

void dash_log_system(const char *msg) {
    char line[LINE_LEN];
    double e = (now_us() - g_start_us) / 1000000.0;
    snprintf(line, sizeof(line), "[%6.3fs] -- %s", e, msg);

    pthread_mutex_lock(&g_lock);
    log_internal(line);
    pthread_mutex_unlock(&g_lock);
}

void dash_wait_for_quit(void) {
    /* Stop the auto-refresh thread, do one final paint, then block on input. */
    g_running = 0;
    pthread_join(g_refresh_tid, NULL);
    render_all();

    nodelay(stdscr, FALSE);
    int ch;
    while ((ch = getch()) != 'q' && ch != 'Q') {
        /* ignore other keys */
    }
}
