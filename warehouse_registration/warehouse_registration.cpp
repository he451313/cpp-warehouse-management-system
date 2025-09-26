#include <iostream>
#include <string>
#include <memory>    // 用於 unique_ptr
#include <limits>    // 用於 cin.ignore
#include <optional>  // 用於可選返回值 (C++17)
#include <sstream>   // 用於 stringstream
#include <vector>    // 用於 std::vector
#include <map>       // 用於 std::map
#include <utility>   // 用於 std::pair

// MySQL Connector/C++ 標頭檔
#include <mysql/jdbc.h>

// 使用 using 來簡化程式碼  
using std::cout;
using std::cin;
using std::endl;
using std::string;
using std::unique_ptr;
using std::optional;
using std::vector;
using std::map;
using std::pair;

// 全域常數，用於取消操作
const string CANCEL_COMMAND = "cancel";

// 資料庫連線資訊 (請根據您的設定修改)
const string DB_HOST = "tcp://127.0.0.1:3306";
const string DB_USER = "root";
const string DB_PASS = "password"; // <--- 在這裡填入你的 MySQL 密碼
const string DB_NAME = "db_name";

// --- 輔助函式原型 ---
optional<string> getUserInput(const string& prompt);
optional<int> getUserInputInt(const string& prompt);

// 函式原型宣告
void showMenu();
void addItemDefinition(sql::Connection* con);
void stockInAndAssignLocation(sql::Connection* con);
void queryItemStock(sql::Connection* con); // <<< 新增函式原型
void showFullInventory(sql::Connection* con);
void removeItemStock(sql::Connection* con);
void deleteItemCompletely(sql::Connection* con);

// 用於儲存聚合後的庫存資料
struct InventoryItem {
    string item_name;
    int total_quantity = 0;
    vector<pair<string, int>> locations;
};

int main() {
    sql::Driver* driver;
    unique_ptr<sql::Connection> con;

    try {
        driver = get_driver_instance();
        con.reset(driver->connect(DB_HOST, DB_USER, DB_PASS));
        con->setSchema(DB_NAME);

        cout << "成功連接到 MySQL 資料庫: " << DB_NAME << endl;
        cout << "提示: 在任何輸入步驟中，輸入 '" << CANCEL_COMMAND << "' 可取消操作返回主選單。" << endl;

        int choice;
        do {
            showMenu();
            cin >> choice;

            if (cin.fail()) {
                cout << "輸入錯誤，請輸入數字。" << endl;
                cin.clear();
                cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                choice = -1;
                continue;
            }
            cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

            switch (choice) {
            case 1: addItemDefinition(con.get()); break;
            case 2: stockInAndAssignLocation(con.get()); break;
            case 3: queryItemStock(con.get()); break; // <<< 新增 case
            case 4: showFullInventory(con.get()); break;
            case 5: removeItemStock(con.get()); break;
            case 6: deleteItemCompletely(con.get()); break;
            case 0: cout << "程式結束。" << endl; break;
            default: cout << "無效的選擇，請重新輸入。" << endl; break;
            }
        } while (choice != 0);

    }
    catch (sql::SQLException& e) {
        cout << "# ERR: SQLException in " << __FILE__;
        cout << "(" << __FUNCTION__ << ") on line " << __LINE__ << endl;
        cout << "# ERR: " << e.what();
        cout << " (MySQL error code: " << e.getErrorCode();
        cout << ", SQLState: " << e.getSQLState() << " )" << endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

// --- 輔助函式實作 ---

optional<string> getUserInput(const string& prompt) {
    cout << prompt;
    string input;
    getline(cin, input);
    if (input == CANCEL_COMMAND) {
        cout << "操作已取消。" << endl;
        return std::nullopt;
    }
    return input;
}

optional<int> getUserInputInt(const string& prompt) {
    while (true) {
        auto input_opt = getUserInput(prompt);
        if (!input_opt) {
            return std::nullopt;
        }
        string input_str = *input_opt;
        std::stringstream ss(input_str);
        int value;
        if (ss >> value && ss.eof()) {
            return value;
        }
        cout << "輸入無效，請輸入一個整數或 '" << CANCEL_COMMAND << "'。" << endl;
    }
}

void showMenu() {
    cout << "\n===== 倉庫管理系統 =====\n";
    cout << "1. 新增物品定義\n";
    cout << "2. 物品入庫並上架\n";
    cout << "3. 查詢物品庫存\n"; // <<< 新增選項
    cout << "4. 顯示完整庫存報表\n";
    cout << "5. 物品出庫 (減少庫存)\n";
    cout << "6. 刪除物品 (包含所有紀錄)\n";
    cout << "0. 離開\n";
    cout << "========================\n";
    cout << "請輸入您的選擇: ";
}

// 1. 新增物品定義
void addItemDefinition(sql::Connection* con) {
    auto item_code_opt = getUserInput("請輸入物品編碼: ");
    if (!item_code_opt) return;
    string item_code = *item_code_opt;

    auto item_name_opt = getUserInput("請輸入物品名稱: ");
    if (!item_name_opt) return;
    string item_name = *item_name_opt;

    if (item_code.empty() || item_name.empty()) {
        cout << "錯誤: 物品編碼和名稱不可為空。" << endl;
        return;
    }

    try {
        unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement("INSERT INTO item_definitions(item_code, item_name) VALUES (?, ?)"));
        pstmt->setString(1, item_code);
        pstmt->setString(2, item_name);
        pstmt->execute();
        cout << "成功新增物品定義: " << item_code << " -> " << item_name << endl;
    }
    catch (sql::SQLException& e) {
        if (e.getErrorCode() == 1062) {
            cout << "新增失敗: 物品編碼 '" << item_code << "' 已存在。" << endl;
        }
        else {
            cout << "新增失敗: " << e.what() << endl;
        }
    }
}

// 2. 物品入庫並上架
void stockInAndAssignLocation(sql::Connection* con) {
    auto item_code_opt = getUserInput("請輸入要入庫的物品編碼: ");
    if (!item_code_opt) return;
    string item_code = *item_code_opt;

    auto location_code_opt = getUserInput("請輸入要上架到的位置編碼: ");
    if (!location_code_opt) return;
    string location_code = *location_code_opt;

    if (item_code.empty() || location_code.empty()) {
        cout << "入庫失敗: 物品編碼和位置編碼皆不可為空。" << endl;
        return;
    }

    auto quantity_opt = getUserInputInt("請輸入入庫數量: ");
    if (!quantity_opt) return;
    int quantity = *quantity_opt;

    if (quantity <= 0) {
        cout << "入庫失敗: 請輸入一個正整數數量。" << endl;
        return;
    }

    try {
        con->setAutoCommit(false);

        unique_ptr<sql::PreparedStatement> pstmt_inv(con->prepareStatement(
            "INSERT INTO inventory (item_code, total_quantity) VALUES (?, ?) "
            "ON DUPLICATE KEY UPDATE total_quantity = total_quantity + ?"
        ));
        pstmt_inv->setString(1, item_code);
        pstmt_inv->setInt(2, quantity);
        pstmt_inv->setInt(3, quantity);
        pstmt_inv->executeUpdate();

        unique_ptr<sql::PreparedStatement> pstmt_loc(con->prepareStatement(
            "INSERT INTO item_locations (item_code, location_code, quantity_at_location) VALUES (?, ?, ?) "
            "ON DUPLICATE KEY UPDATE quantity_at_location = quantity_at_location + ?"
        ));
        pstmt_loc->setString(1, item_code);
        pstmt_loc->setString(2, location_code);
        pstmt_loc->setInt(3, quantity);
        pstmt_loc->setInt(4, quantity);
        pstmt_loc->executeUpdate();

        con->commit();
        con->setAutoCommit(true);
        cout << "成功將 " << quantity << " 件 " << item_code << " 入庫並放置於 " << location_code << endl;
    }
    catch (sql::SQLException& e) {
        cout << "入庫上架操作失敗: ";
        if (e.getErrorCode() == 1452) {
            cout << "請先確認物品編碼 '" << item_code << "' 是否存在於物品定義中。" << endl;
        }
        else {
            cout << e.what() << endl;
        }
        try {
            con->rollback();
            con->setAutoCommit(true);
            cout << "資料庫操作已復原。" << endl;
        }
        catch (sql::SQLException& e_rollback) {
            cout << "復原交易時發生嚴重錯誤: " << e_rollback.what() << endl;
        }
    }
}

// 3. 查詢單一物品庫存 (新函式)
void queryItemStock(sql::Connection* con) {
    auto item_code_opt = getUserInput("請輸入要查詢的物品編碼: ");
    if (!item_code_opt) return;
    string item_code_to_query = *item_code_opt;

    if (item_code_to_query.empty()) {
        cout << "查詢失敗: 物品編碼不可為空。" << endl;
        return;
    }

    try {
        unique_ptr<sql::PreparedStatement> pstmt(con->prepareStatement(
            "SELECT d.item_name, i.total_quantity, l.location_code, l.quantity_at_location "
            "FROM item_definitions d "
            "LEFT JOIN inventory i ON d.item_code = i.item_code "
            "LEFT JOIN item_locations l ON d.item_code = l.item_code "
            "WHERE d.item_code = ? "
            "ORDER BY l.location_code"
        ));
        pstmt->setString(1, item_code_to_query);
        unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

        if (res->rowsCount() == 0) {
            cout << "找不到物品編碼 '" << item_code_to_query << "'。" << endl;
            return;
        }

        // 聚合單一物品的資料
        InventoryItem item;
        bool item_info_set = false;
        while (res->next()) {
            if (!item_info_set) {
                item.item_name = res->getString("item_name").asStdString();
                if (!res->isNull("total_quantity")) {
                    item.total_quantity = res->getInt("total_quantity");
                }
                item_info_set = true;
            }
            if (!res->isNull("location_code")) {
                string loc_code = res->getString("location_code").asStdString();
                if (!loc_code.empty()) {
                    item.locations.push_back({ loc_code, res->getInt("quantity_at_location") });
                }
            }
        }

        // 輸出查詢結果
        cout << "\n----------- 物品庫存查詢結果 -----------" << endl;
        cout << "編碼\t\t: " << item_code_to_query << endl;
        cout << "名稱\t\t: " << item.item_name << endl;
        cout << "總庫存\t\t: " << item.total_quantity << endl;
        cout << "位置: 數量\t: ";

        if (item.locations.empty()) {
            cout << "-" << endl;
        }
        else {
            cout << item.locations[0].first << ": " << item.locations[0].second << endl;
            for (size_t i = 1; i < item.locations.size(); ++i) {
                cout << "\t\t  " // 對齊用的前導空白
                    << item.locations[i].first << ": " << item.locations[i].second << endl;
            }
        }
        cout << "----------------------------------------" << endl;
    }
    catch (sql::SQLException& e) {
        cout << "查詢失敗: " << e.what() << endl;
    }
}

// 4. 顯示完整庫存報表
void showFullInventory(sql::Connection* con) {
    try {
        unique_ptr<sql::Statement> stmt(con->createStatement());
        unique_ptr<sql::ResultSet> res(stmt->executeQuery(
            "SELECT d.item_code, d.item_name, i.total_quantity, l.location_code, l.quantity_at_location "
            "FROM item_definitions d "
            "LEFT JOIN inventory i ON d.item_code = i.item_code "
            "LEFT JOIN item_locations l ON d.item_code = l.item_code "
            "ORDER BY d.item_code, l.location_code"
        ));

        map<string, InventoryItem> inventoryData;
        while (res->next()) {
            string code = res->getString("item_code").asStdString();

            if (inventoryData.find(code) == inventoryData.end()) {
                inventoryData[code].item_name = res->getString("item_name").asStdString();
                if (!res->isNull("total_quantity")) {
                    inventoryData[code].total_quantity = res->getInt("total_quantity");
                }
            }
            if (!res->isNull("location_code")) {
                string loc_code = res->getString("location_code").asStdString();
                if (!loc_code.empty()) {
                    inventoryData[code].locations.push_back({ loc_code, res->getInt("quantity_at_location") });
                }
            }
        }

        cout << "\n------------------------- 完整庫存報表 -------------------------" << endl;
        cout << "編碼\t\t名稱\t\t總庫存\t\t位置: 數量" << endl;
        cout << "----------------------------------------------------------------" << endl;

        if (inventoryData.empty()) {
            cout << "資料庫中目前沒有任何物品定義。" << endl;
        }
        else {
            for (const auto& pair : inventoryData) {
                const string& item_code = pair.first;
                const InventoryItem& item = pair.second;

                cout << item_code << "\t\t"
                    << item.item_name << "\t\t"
                    << item.total_quantity << "\t\t";

                if (item.locations.empty()) {
                    cout << "-" << endl;
                }
                else {
                    cout << item.locations[0].first << ": " << item.locations[0].second << endl;
                    for (size_t i = 1; i < item.locations.size(); ++i) {
                        cout << "\t\t\t\t\t\t"
                            << item.locations[i].first << ": " << item.locations[i].second << endl;
                    }
                }
            }
        }
        cout << "----------------------------------------------------------------" << endl;
    }
    catch (sql::SQLException& e) {
        cout << "查詢失敗: " << e.what() << endl;
    }
}

// 5. 物品出庫 (減少庫存)
void removeItemStock(sql::Connection* con) {
    auto item_code_opt = getUserInput("請輸入要出庫的物品編碼: ");
    if (!item_code_opt) return;
    string item_code = *item_code_opt;

    auto location_code_opt = getUserInput("請輸入要從哪個位置出庫: ");
    if (!location_code_opt) return;
    string location_code = *location_code_opt;

    auto quantity_opt = getUserInputInt("請輸入出庫數量: ");
    if (!quantity_opt) return;
    int quantity_to_remove = *quantity_opt;

    if (quantity_to_remove <= 0) {
        cout << "出庫失敗: 請輸入一個正整數數量。" << endl;
        return;
    }

    int current_quantity_at_location = -1;
    int current_total_quantity = -1;
    try {
        unique_ptr<sql::PreparedStatement> pstmt_check(con->prepareStatement(
            "SELECT i.total_quantity, l.quantity_at_location "
            "FROM item_definitions d "
            "LEFT JOIN inventory i ON d.item_code = i.item_code "
            "LEFT JOIN item_locations l ON d.item_code = l.item_code AND l.location_code = ? "
            "WHERE d.item_code = ?"
        ));
        pstmt_check->setString(1, location_code);
        pstmt_check->setString(2, item_code);
        unique_ptr<sql::ResultSet> res(pstmt_check->executeQuery());
        if (res->next()) {
            if (!res->isNull("total_quantity")) current_total_quantity = res->getInt("total_quantity");
            if (!res->isNull("quantity_at_location")) current_quantity_at_location = res->getInt("quantity_at_location");
        }
        else {
            cout << "出庫失敗: 物品編碼 '" << item_code << "' 不存在。" << endl;
            return;
        }
        if (current_quantity_at_location < quantity_to_remove) {
            cout << "出庫失敗: 位置 '" << location_code << "' 的庫存 (" << (current_quantity_at_location == -1 ? 0 : current_quantity_at_location)
                << ") 不足。" << endl;
            return;
        }
        if (current_total_quantity < quantity_to_remove) {
            cout << "出庫失敗: 總庫存 (" << (current_total_quantity == -1 ? 0 : current_total_quantity)
                << ") 不足。資料可能存在不一致。" << endl;
            return;
        }
    }
    catch (sql::SQLException& e) {
        cout << "檢查庫存時發生錯誤: " << e.what() << endl;
        return;
    }

    try {
        con->setAutoCommit(false);
        if (current_quantity_at_location == quantity_to_remove) {
            unique_ptr<sql::PreparedStatement> pstmt_loc(con->prepareStatement(
                "DELETE FROM item_locations WHERE item_code = ? AND location_code = ?"
            ));
            pstmt_loc->setString(1, item_code);
            pstmt_loc->setString(2, location_code);
            pstmt_loc->execute();
        }
        else {
            unique_ptr<sql::PreparedStatement> pstmt_loc(con->prepareStatement(
                "UPDATE item_locations SET quantity_at_location = quantity_at_location - ? WHERE item_code = ? AND location_code = ?"
            ));
            pstmt_loc->setInt(1, quantity_to_remove);
            pstmt_loc->setString(2, item_code);
            pstmt_loc->setString(3, location_code);
            pstmt_loc->execute();
        }
        unique_ptr<sql::PreparedStatement> pstmt_inv(con->prepareStatement(
            "UPDATE inventory SET total_quantity = total_quantity - ? WHERE item_code = ?"
        ));
        pstmt_inv->setInt(1, quantity_to_remove);
        pstmt_inv->setString(2, item_code);
        pstmt_inv->execute();
        con->commit();
        con->setAutoCommit(true);
        cout << "成功從位置 '" << location_code << "' 出庫 " << quantity_to_remove << " 件物品 '" << item_code << "'。" << endl;
    }
    catch (sql::SQLException& e) {
        cout << "出庫操作失敗: " << e.what() << endl;
        try {
            con->rollback();
            con->setAutoCommit(true);
            cout << "資料庫操作已復原。" << endl;
        }
        catch (sql::SQLException& e_rollback) {
            cout << "復原交易時發生嚴重錯誤: " << e_rollback.what() << endl;
        }
    }
}

// 6. 刪除物品 (包含所有紀錄)
void deleteItemCompletely(sql::Connection* con) {
    auto item_code_opt = getUserInput("請輸入要【永久刪除】的物品編碼: ");
    if (!item_code_opt) return;
    string item_code = *item_code_opt;

    if (item_code.empty()) {
        cout << "錯誤: 物品編碼不可為空。" << endl;
        return;
    }

    try {
        unique_ptr<sql::PreparedStatement> pstmt_check(con->prepareStatement("SELECT item_name FROM item_definitions WHERE item_code = ?"));
        pstmt_check->setString(1, item_code);
        unique_ptr<sql::ResultSet> res(pstmt_check->executeQuery());
        if (!res->next()) {
            cout << "找不到物品編碼 '" << item_code << "'，無法刪除。" << endl;
            return;
        }
        string item_name = res->getString("item_name").asStdString();

        char confirmation = ' ';
        while (confirmation != 'y' && confirmation != 'Y' && confirmation != 'n' && confirmation != 'N') {
            cout << "【警告】您確定要永久刪除物品 '" << item_name << "' (編碼: " << item_code << ") 嗎?" << endl;
            cout << "這將會移除其【所有的】庫存和位置紀錄，此操作無法復原。 (y/n): ";
            cin >> confirmation;
            cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }

        if (confirmation != 'y' && confirmation != 'Y') {
            cout << "已取消刪除操作。" << endl;
            return;
        }
    }
    catch (sql::SQLException& e) {
        cout << "在檢查物品時發生錯誤: " << e.what() << endl;
        return;
    }

    try {
        con->setAutoCommit(false);
        unique_ptr<sql::PreparedStatement> pstmt_loc(con->prepareStatement("DELETE FROM item_locations WHERE item_code = ?"));
        pstmt_loc->setString(1, item_code);
        pstmt_loc->execute();
        unique_ptr<sql::PreparedStatement> pstmt_inv(con->prepareStatement("DELETE FROM inventory WHERE item_code = ?"));
        pstmt_inv->setString(1, item_code);
        pstmt_inv->execute();
        unique_ptr<sql::PreparedStatement> pstmt_def(con->prepareStatement("DELETE FROM item_definitions WHERE item_code = ?"));
        pstmt_def->setString(1, item_code);
        pstmt_def->execute();
        con->commit();
        con->setAutoCommit(true);
        cout << "已成功刪除物品 '" << item_code << "' 及其所有相關紀錄。" << endl;
    }
    catch (sql::SQLException& e) {
        cout << "刪除失敗: " << e.what() << endl;
        try {
            con->rollback();
            con->setAutoCommit(true);
            cout << "資料庫操作已復原。" << endl;
        }
        catch (sql::SQLException& e_rollback) {
            cout << "復原交易時發生嚴重錯誤: " << e_rollback.what() << endl;
        }
    }
}