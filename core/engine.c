#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sqlite3.h>

#define MAX_LINE_LEN 512

typedef struct {
    char sector[32];
    char profile_name[32];
    double max_variance;
    double spoilage_rate;
} EngineConfig;

typedef struct {
    char receipt_no[16];
    char completion_time[24];
    char details[128];
    char status[16];
    double paid_in;
    double withdrawn;
    double balance;
} MpesaTransaction;

time_t parse_timestamp(const char *time_str) {
    struct tm t;
    memset(&t, 0, sizeof(struct tm));
    if (sscanf(time_str, "%d-%d-%d %d:%d:%d", 
               &t.tm_year, &t.tm_mon, &t.tm_mday, 
               &t.tm_hour, &t.tm_min, &t.tm_sec) == 6) {
        t.tm_year -= 1900;
        t.tm_mon -= 1;
        return mktime(&t);
    }
    return 0;
}

void clean_string(char *dest, const char *src, size_t max_len) {
    size_t i = 0, j = 0;
    while (src[i] && j < max_len - 1) {
        if (src[i] != '"' && src[i] != '\r' && src[i] != '\n') {
            dest[j++] = src[i];
        }
        i++;
    }
    dest[j] = '\0';
}

int load_profile(const char *filename, EngineConfig *config) {
    FILE *file = fopen(filename, "r");
    if (!file) return -1;
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (strstr(line, "\"sector\"")) sscanf(line, " %*[^:]: \"%[^\"]\"", config->sector);
        else if (strstr(line, "\"profile_name\"")) sscanf(line, " %*[^:]: \"%[^\"]\"", config->profile_name);
        else if (strstr(line, "\"shift_variance_max_allowed_kes\"")) sscanf(line, " %*[^:]: %lf", &config->max_variance);
        else if (strstr(line, "\"acceptable_spoilage_percentage\"")) sscanf(line, " %*[^:]: %lf", &config->spoilage_rate);
    }
    fclose(file);
    return 0;
}

void save_to_vault(sqlite3 *db, MpesaTransaction *tx, const char *status_label) {
    char query[512];
    char *err_msg = 0;
    snprintf(query, sizeof(query),
             "INSERT OR IGNORE INTO financial_transactions "
             "(universal_tx_id, source_channel, gross_amount, reported_fees, transaction_time, reconciliation_status) "
             "VALUES ('%s', 'MPESA_TILL', %.2f, %.2f, '%s', '%s');",
             tx->receipt_no, tx->paid_in, tx->withdrawn, tx->completion_time, status_label);
    sqlite3_exec(db, query, 0, 0, &err_msg);
    if (err_msg) sqlite3_free(err_msg);
}

void process_transaction_line(char *line, sqlite3 *db) {
    MpesaTransaction tx;
    memset(&tx, 0, sizeof(MpesaTransaction));
    char *token = strtok(line, ",");
    int column = 0;

    while (token != NULL) {
        switch(column) {
            case 0: clean_string(tx.receipt_no, token, sizeof(tx.receipt_no)); break;
            case 1: clean_string(tx.completion_time, token, sizeof(tx.completion_time)); break;
            case 2: clean_string(tx.details, token, sizeof(tx.details)); break;
            case 3: clean_string(tx.status, token, sizeof(tx.status)); break;
            case 4: tx.paid_in = atof(token); break;
            case 5: tx.withdrawn = atof(token); break;
            case 6: tx.balance = atof(token); break;
        }
        token = strtok(NULL, ",");
        column++;
    }

    if (strlen(tx.receipt_no) == 0) return;
    if (strcmp(tx.status, "Completed") != 0) {
        save_to_vault(db, &tx, "MISMATCH_LEAKAGE");
        return;
    }
    save_to_vault(db, &tx, "MATCHED");
}

double calculate_expected_inventory_value(const char *filename) {
    FILE *stream = fopen(filename, "r");
    if (!stream) return 0.0;

    char line[MAX_LINE_LEN];
    double total_value = 0.0;

    // Default column indices (-1 means "not found yet")
    int item_col_idx = -1;
    int qty_col_idx = -1;
    int price_col_idx = -1;

    // 1. Read the first line (The Header Row) to dynamically map columns
    if (fgets(line, sizeof(line), stream)) {
        char header_line[MAX_LINE_LEN];
        strncpy(header_line, line, MAX_LINE_LEN);
        header_line[strcspn(header_line, "\r\n")] = 0;

        char *token = strtok(header_line, ",");
        int current_col = 0;

        while (token != NULL) {
            char clean_token[64];
            clean_string(clean_token, token, sizeof(clean_token));

    // Force token to lowercase by shifting ASCII values
                for(int i = 0; clean_token[i] != '\0'; i++) {
                    if(clean_token[i] >= 'A' && clean_token[i] <= 'Z') {
                        clean_token[i] = clean_token[i] + 32;
                    }
                }

            // Dynamically detect crucial header tags regardless of position or extra columns
      // Replace that specific if/else block inside the header parsing loop with this:
      if (strstr(clean_token, "item") || strstr(clean_token, "product") || strstr(clean_token, "medication")) {
          item_col_idx = current_col;
      } else if (strstr(clean_token, "quantity") || strstr(clean_token, "qty") || strstr(clean_token, "count")) {
         qty_col_idx = current_col;
      } else if (strstr(clean_token, "price") || strstr(clean_token, "cost") || strstr(clean_token, "rate")) {
         price_col_idx = current_col;
}
            token = strtok(NULL, ",");
            current_col++;
        }
    }

    // Fallback security check: If basic columns aren't identified, default to legacy positioning
    if (item_col_idx == -1) item_col_idx = 0;
    if (qty_col_idx == -1) qty_col_idx = 1;
    if (price_col_idx == -1) price_col_idx = 2;

    // 2. Process data rows using the dynamic column map
    while (fgets(line, sizeof(line), stream)) {
        line[strcspn(line, "\r\n")] = 0;
        if (strlen(line) == 0) continue;

        char data_line[MAX_LINE_LEN];
        strncpy(data_line, line, MAX_LINE_LEN);

        double quantity = 0.0;
        double unit_price = 0.0;
        int current_col = 0;

        char *token = strtok(data_line, ",");
        while (token != NULL) {
            if (current_col == qty_col_idx) {
                quantity = atof(token);
            } else if (current_col == price_col_idx) {
                unit_price = atof(token);
            }
            token = strtok(NULL, ",");
            current_col++;
        }
        total_value += (quantity * unit_price);
    }

    fclose(stream);
    return total_value;
} //
int main() {
    // your main function follows right after...
EngineConfig config = {0};
    sqlite3 *db;
    
    if (load_profile("profiles/kitchen_profile.json", &config) != 0 || sqlite3_open("storage/vault.db", &db) != SQLITE_OK) {
        return 1;
    }

    FILE *stream = fopen("tests/mock_statement.csv", "r");
    if (!stream) {
        sqlite3_close(db);
        return 1;
    }
    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), stream)) {
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) > 0) process_transaction_line(line, db);
    }
    fclose(stream);

    double expected_cash = calculate_expected_inventory_value("tests/mock_inventory.csv");

    sqlite3_stmt *stmt;
    double actual_cash = 0.0;
    if (sqlite3_prepare_v2(db, "SELECT SUM(gross_amount) FROM financial_transactions WHERE reconciliation_status = 'MATCHED';", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            actual_cash = sqlite3_column_double(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    double raw_variance = expected_cash - actual_cash;
    double allowed_spoilage_loss = expected_cash * (config.spoilage_rate / 100.0);
    double unexplained_leakage = raw_variance - allowed_spoilage_loss;

    // ─── TERMINAL CONSOLE DASHBOARD UI HEADER ──────────────────────────────────
    printf("\033[H\033[J"); // Clears the screen dynamically
    printf("┌────────────────────────────────────────────────────────┐\n");
    printf("│        SYSTEM STATE STATE-RECONCILIATION AUDIT         │\n");
    printf("├────────────────────────────────────────────────────────┤\n");
    printf("│ Active Sector Profile : %-31s │\n", config.sector);
    printf("│ Policy Framework      : %-31s │\n", config.profile_name);
    printf("├────────────────────────────────────────────────────────┤\n");
    printf("│ Expected Stock Retail : KSh %-26.2f │\n", expected_cash);
    printf("│ Actual Verified Inflow: KSh %-26.2f │\n", actual_cash);
    printf("│ Gross In-Day Variance : KSh %-26.2f │\n", raw_variance);
    printf("│ Profile Spoilage Limit: KSh %-26.2f │\n", allowed_spoilage_loss);
    printf("├────────────────────────────────────────────────────────┤\n");
    
    if (unexplained_leakage > config.max_variance) {
        printf("│ STATUS: 🚨 UNEXPLAINED FIN-LEAKAGE: KSh %-14.2f │\n", unexplained_leakage);
    } else {
        printf("│ STATUS: ✅ LEAKAGE WITHIN SAFE BUSINESS TOLERANCE      │\n");
    }
    printf("└────────────────────────────────────────────────────────┘\n");

    const char *mock_dispatch_time = "2026-03-10 11:30:00"; 
    time_t dispatch_t = parse_timestamp(mock_dispatch_time);

    const char *sql = "SELECT universal_tx_id, transaction_time FROM financial_transactions WHERE reconciliation_status = 'MATCHED' ORDER BY transaction_time ASC LIMIT 1;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *tx_id = (const char *)sqlite3_column_text(stmt, 0);
            const char *tx_time_str = (const char *)sqlite3_column_text(stmt, 1);
            time_t tx_t = parse_timestamp(tx_time_str);
            double diff_minutes = difftime(tx_t, dispatch_t) / 60.0;

            printf("┌────────────────────────────────────────────────────────┐\n");
            printf("│               TEMPORAL LATENCY TIMELINE               │\n");
            printf("├────────────────────────────────────────────────────────┤\n");
            printf("│ Dispatch Baseline     : %-31s │\n", mock_dispatch_time);
            printf("│ Inflow Confirmation   : %-31s │\n", tx_time_str);
            printf("│ Reconciliation Delay  : %-26.1f Minutes │\n", diff_minutes);
            printf("└────────────────────────────────────────────────────────┘\n");
        }
        sqlite3_finalize(stmt);
    }

    char *err_msg = 0;
    const char *purge_query = "DELETE FROM financial_transactions WHERE reconciliation_status = 'MATCHED' AND transaction_time < datetime('now', '-30 days');";
    sqlite3_exec(db, purge_query, 0, 0, &err_msg);
    sqlite3_exec(db, "VACUUM;", 0, 0, &err_msg);

    sqlite3_close(db);
    return 0;
}
