#include <iostream>
#include <mysqlx/xdevapi.h>

using namespace mysqlx;

// --- 設定您的 MySQL 連線資訊 ---
const std::string MYSQL_HOST = "localhost";
const std::string MYSQL_USER = "your_user";      // <-- 請填寫您的 MySQL 使用者名稱
const std::string MYSQL_PASSWORD = "your_password";  // <-- 請填寫您的 MySQL 密碼
const std::string DB_SCHEMA = "warehouse_db";

// 函數原型
void setup_database(Session& sql);
void insert_sample_data(Schema& db);
void query_and_print_inventory(Session& sql);

int main() {
    try {
        std::cout << "Connecting to MySQL server at " << MYSQL_HOST << "..." << std::endl;
        Session sql(MYSQL_HOST, 33060, MYSQL_USER, MYSQL_PASSWORD);
        std::cout << "Connection successful!" << std::endl;

        setup_database(sql);
        Schema db = sql.getSchema(DB_SCHEMA);
        insert_sample_data(db);
        query_and_print_inventory(sql);

    } catch (const mysqlx::Error &err) {
        std::cerr << "ERROR: " << err << std::endl;
        std::cerr << "Please make sure the MySQL server is running and the connection details are correct." << std::endl;
        return 1;
    } catch (std::exception &ex) {
        std::cerr << "STD EXCEPTION: " << ex.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown exception caught" << std::endl;
        return 1;
    }
    return 0;
}

void setup_database(Session& sql) {
    std::cout << "Creating schema and tables if they don't exist..." << std::endl;
    sql.sql("CREATE DATABASE IF NOT EXISTS " + DB_SCHEMA).execute();
    sql.sql("USE " + DB_SCHEMA).execute();

    sql.sql(R"(
        CREATE TABLE IF NOT EXISTS item_codes (
            item_code VARCHAR(50) PRIMARY KEY,
            item_name VARCHAR(255) NOT NULL,
            description TEXT
        )
    )").execute();

    sql.sql(R"(
        CREATE TABLE IF NOT EXISTS inventory (
            id INT AUTO_INCREMENT PRIMARY KEY,
            item_code VARCHAR(50),
            location VARCHAR(100) NOT NULL,
            quantity INT NOT NULL,
            last_updated TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
            FOREIGN KEY (item_code) REFERENCES item_codes(item_code)
        )
    )").execute();
    std::cout << "Schema and tables are ready." << std::endl;
}

void insert_sample_data(Schema& db) {
    std::cout << "Inserting sample data..." << std::endl;
    Table item_codes_table = db.getTable("item_codes");
    Table inventory_table = db.getTable("inventory");

    // 清空舊資料 (新版 API 正確語法)
    item_codes_table.remove().execute();
    inventory_table.remove().execute();

    item_codes_table.insert("item_code", "item_name", "description")
        .values("CPU-I7-12700K", "Intel Core i7-12700K", "12th Gen Intel Processor")
        .values("GPU-RTX-3080", "NVIDIA GeForce RTX 3080", "10GB GDDR6X Graphics Card")
        .values("RAM-DDR5-32G", "Corsair Vengeance DDR5 32GB", "2x16GB, 5200MHz")
        .execute();

    inventory_table.insert("item_code", "location", "quantity")
        .values("CPU-I7-12700K", "Shelf A, Row 1", 50)
        .values("GPU-RTX-3080", "Shelf B, Row 3", 25)
        .values("RAM-DDR5-32G", "Shelf A, Row 2", 100)
        .execute();

    std::cout << "Sample data inserted." << std::endl;
}

void query_and_print_inventory(Session& sql) {
    std::cout << "\n--- Current Warehouse Inventory ---" << std::endl;
    
    SqlResult result = sql.sql(R"(
        SELECT
            inv.id,
            inv.item_code,
            ic.item_name,
            inv.location,
            inv.quantity,
            inv.last_updated
        FROM
            inventory AS inv
        JOIN
            item_codes AS ic ON inv.item_code = ic.item_code
        ORDER BY
            inv.location
    )").execute();

    for (Row row : result.fetchAll()) {
        std::cout << "ID: " << row[0]
                  << ", Name: " << row[2]
                  << ", Code: " << row[1]
                  << ", Location: " << row[3]
                  << ", Quantity: " << row[4]
                  << std::endl;
    }
    std::cout << "--- End of Report ---" << std::endl;
}

