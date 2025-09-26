# C++ 簡易倉庫管理系統 (Simple Warehouse Management System in C++)

這是一個使用 C++ 和 MySQL Connector/C++ 編寫的簡易主控台倉庫管理系統。使用者可以透過命令列介面，對物品庫存進行基本的增、刪、改、查操作。

## 主要功能

*   **物品定義**：新增物品的基本資料（編碼 -> 名稱）。
*   **入庫與上架**：將指定數量的物品入庫，並同時分配到特定的存放位置。
*   **庫存查詢**：
    *   查詢單一物品的詳細庫存資訊。
    *   顯示所有物品的完整庫存報表。
*   **物品出庫**：從指定的存放位置取出特定數量的物品，並同步更新總庫存。
*   **刪除物品**：永久刪除一個物品的定義及其所有相關的庫存和位置紀錄。
*   **操作取消**：在任何輸入步驟中，輸入 `cancel` 即可取消當前操作並返回主選單。

## 技術棧

*   **語言**: C++17
*   **資料庫**: MySQL
*   **連接器**: MySQL Connector/C++ 8.0+
*   **開發環境**: Visual Studio 2022 (Windows)

---

## 如何在 Visual Studio 2022 中建置與執行

本專案需要在本地環境設定 MySQL Connector/C++ 函式庫。以下為詳細步驟。

### 前置準備

1.  **安裝 MySQL Server**：確保您的電腦上已安裝並正在執行 MySQL 伺服器。
2.  **安裝 MySQL Connector/C++**：
    *   前往 [MySQL Connector/C++ 官方下載頁面](https://dev.mysql.com/downloads/connector/cpp/)。
    *   下載與您的作業系統及架構（例如 Windows x64）對應的 ZIP 或 MSI 安裝包。
    *   **記下安裝路徑**，預設通常在 `C:\Program Files\MySQL\MySQL Connector C++ 8.0`。

3.  **建立資料庫與資料表**：
    *   登入您的 MySQL 伺服器。
    *   執行以下 SQL 指令碼來建立所需的資料庫和資料表：
      ```sql
      CREATE DATABASE IF NOT EXISTS warehouse_db;
      USE warehouse_db;

      CREATE TABLE IF NOT EXISTS item_definitions (
          item_code VARCHAR(50) PRIMARY KEY,
          item_name VARCHAR(255) NOT NULL
      );

      CREATE TABLE IF NOT EXISTS inventory (
          item_code VARCHAR(50) PRIMARY KEY,
          total_quantity INT NOT NULL DEFAULT 0,
          FOREIGN KEY (item_code) REFERENCES item_definitions(item_code)
      );

      CREATE TABLE IF NOT EXISTS item_locations (
          item_code VARCHAR(50),
          location_code VARCHAR(50),
          quantity_at_location INT NOT NULL DEFAULT 0,
          PRIMARY KEY (item_code, location_code),
          FOREIGN KEY (item_code) REFERENCES item_definitions(item_code)
      );
      ```

### Visual Studio 專案設定

打開 `.sln` 專案檔後，需要進行以下設定才能成功編譯。

1.  **設定平台為 x64**：
    *   在 Visual Studio 工具列上，將方案平台從 `x86` 切換為 `x64`。

2.  **開啟專案屬性**：
    *   在「方案總管」中，對您的專案（例如 `warehouse_registration`）按右鍵，選擇 **「屬性」**。

3.  **設定 C/C++ Include 目錄**：
    *   導覽到 **「組態屬性」 -> 「C/C++」 -> 「一般」**。
    *   編輯 **「額外的 Include 目錄」**，加入 Connector/C++ 的 `include` 資料夾路徑，例如：
      `C:\Program Files\MySQL\MySQL Connector C++ 8.0\include`

4.  **設定連結器程式庫目錄**：
    *   導覽到 **「組態屬性」 -> 「連結器」 -> 「一般」**。
    *   編輯 **「額外的程式庫目錄」**，加入 Connector/C++ 的 `lib64` 資料夾路徑，例如：
      `C:\Program Files\MySQL\MySQL Connector C++ 8.0\lib64`

5.  **設定連結器相依性**：
    *   導覽到 **「組態屬性」 -> 「連結器」 -> 「輸入」**。
    *   編輯 **「額外的相依性」**，在清單的開頭加入 `mysqlcppconn.lib`。

6.  **(重要) 設定執行階段程式庫**：
    *   為了避免執行階段函式庫不匹配導致的崩潰，需要統一設定。
    *   導覽到 **「組態屬性」 -> 「C/C++」 -> 「程式碼產生」**。
    *   將 **「執行階段程式庫」** 設定為 **「多執行緒 DLL (/MD)」**（適用於 Release 組態）或 **「多執行緒偵錯 DLL (/MDd)」**。
    *   將 Debug 組態也設定為 **「多執行緒 DLL (/MD)」**，並將對應的預處理器定義從 `_DEBUG` 改為 `NDEBUG`。

### 執行程式

1.  **修改連線資訊**：
    *   打開 `warehouse_registration.cpp` 檔案。
    *   找到以下幾行，並根據您的 MySQL 設定修改密碼等資訊：
      ```cpp
      const string DB_PASS = "your_password_here";
      const string DB_NAME = "warehouse_db";
      ```

2.  **複製必要的 DLL 檔案**：
    *   從 Connector/C++ 的 `lib64` 資料夾（例如 `C:\Program Files\MySQL\MySQL Connector C++ 8.0\lib64`）中，複製以下檔案：
        *   `mysqlcppconn.dll`
        *   `libcrypto-*.dll`
        *   `libssl-*.dll`
    *   將這些 DLL 檔案貼到您專案的 **輸出目錄** 中，也就是 `.exe` 執行檔所在的資料夾（通常是專案目錄下的 `x64\Debug` 或 `x64\Release`）。

3.  **建置並執行**：
    *   現在您可以點擊 Visual Studio 的「本機 Windows 偵錯工具」按鈕來建置並執行程式了。
