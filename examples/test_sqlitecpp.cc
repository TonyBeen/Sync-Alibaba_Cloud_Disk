/*************************************************************************
    > File Name: test_sqlitecpp.cc
    > Author: hsz
    > Brief:
    > Created Time: 2024年10月16日 星期三 14时45分59秒
 ************************************************************************/

#include <iostream>
#include <SQLiteCpp/SQLiteCpp.h>
#include <sqlite3.h>

#define DB_NAME "backup.db"

void attach_memory_db()
{
    try {
        // 创建内存数据库
        SQLite::Database memoryDb(":memory:", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
        std::cout << "In-memory database created." << std::endl;

        // 在内存中创建表并插入数据
        memoryDb.exec("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER);");
        memoryDb.exec("INSERT INTO users (name, age) VALUES ('Alice', 30);");
        memoryDb.exec("INSERT INTO users (name, age) VALUES ('Bob', 25);");
        std::cout << "Data inserted into in-memory table." << std::endl;

        // 附加磁盘上的数据库
        memoryDb.exec("ATTACH '" DB_NAME "' AS diskDb;");
        std::cout << "Disk database attached as 'diskDb'." << std::endl;

        // 在磁盘数据库中创建表，并将内存表中的数据复制到磁盘表
        memoryDb.exec("DROP TABLE IF EXISTS diskDb.users");
        memoryDb.exec("CREATE TABLE diskDb.users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER);");
        memoryDb.exec("INSERT INTO diskDb.users SELECT * FROM users;");

        std::cout << "Data copied from in-memory table to disk table." << std::endl;

        // 关闭附加的磁盘数据库
        memoryDb.exec("DETACH diskDb;");
        std::cout << "Disk database detached." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "SQLite error: " << e.what() << std::endl;
    }

    // 读取数据库
    try {
        SQLite::Database diskDb(DB_NAME, SQLite::OPEN_READWRITE);

        // 获取数据库的文本编码方式
        std::string encoding = diskDb.execAndGet("PRAGMA encoding").getText();

        // 输出编码方式 UTF-8
        std::cout << "The TEXT encoding of the database is: " << encoding << std::endl;

        // 从磁盘数据库读取数据
        SQLite::Statement query(diskDb, "SELECT * FROM users;");
        while (query.executeStep()) {
            printf("Id: %d, Name: %s, Age: %d\n", query.getColumn(0).getInt(), query.getColumn(1).getText(), query.getColumn(2).getInt());
        }
    } catch(const std::exception& e) {
        std::cerr << e.what() << '\n';
    }

    std::remove(DB_NAME);
}

// 手动复制数据到磁盘数据库
void copy_memory_db()
{
    try {
        // 创建内存数据库
        SQLite::Database memoryDb(":memory:", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
        std::cout << "In-memory database created." << std::endl;

        // 在内存中创建表并插入数据
        memoryDb.exec("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER);");
        memoryDb.exec("INSERT INTO users (name, age) VALUES ('Alice', 30);");
        memoryDb.exec("INSERT INTO users (name, age) VALUES ('Bob', 25);");
        std::cout << "Data inserted into in-memory table." << std::endl;

        // 创建磁盘数据库
        SQLite::Database diskDb("backup.db", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
        std::cout << "Disk database created." << std::endl;

        diskDb.exec("DROP TABLE IF EXISTS users");
        // 在磁盘数据库中创建表
        diskDb.exec("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER);");

        // 从内存数据库读取数据
        SQLite::Statement query(memoryDb, "SELECT * FROM users;");
        while (query.executeStep()) {
            // 插入数据到磁盘数据库
            SQLite::Statement insert(diskDb, "INSERT INTO users (id, name, age) VALUES (?, ?, ?);");
            insert.bind(1, query.getColumn(0).getInt());  // ID
            insert.bind(2, query.getColumn(1).getText()); // Name
            insert.bind(3, query.getColumn(2).getInt());  // Age
            insert.exec();
        }
        std::cout << "Data copied to disk database." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "SQLite error: " << e.what() << std::endl;
    }
}

void open_nonexistent_db()
{
    try {
        // 尝试以只读模式打开一个不存在的数据库
        SQLite::Database db("nonexistent.db", SQLite::OPEN_READONLY);
        std::cout << "Database opened successfully" << std::endl;
    }
    catch (std::exception& e) {
        // 如果文件不存在，会抛出异常
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}

void test_attach_memory_db()
{
    try {
        // 创建磁盘数据库
        SQLite::Database diskDb(DB_NAME, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
        std::cout << "Disk database created." << std::endl;

        std::cout << diskDb.exec("ATTACH '" DB_NAME "' AS backup ;") << std::endl;

        std::cout << diskDb.exec("DROP TABLE IF EXISTS backup.users") << std::endl;

        // 在磁盘数据库中创建表
        std::cout << diskDb.exec("CREATE TABLE backup.users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER);") << std::endl;
        std::cout << diskDb.exec("INSERT INTO backup.users (name, age) VALUES ('Alice', 30);") << std::endl;

        // 附加磁盘上的数据库
        diskDb.exec("ATTACH ':memory:' AS memory;");
        std::cout << "memory database attached as 'memory'." << std::endl;

        // 在磁盘数据库中创建表，并将内存表中的数据复制到磁盘表
        diskDb.exec("DROP TABLE IF EXISTS memory.users");
        diskDb.exec("CREATE TABLE memory.users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER);");
        diskDb.exec("INSERT INTO memory.users SELECT * FROM users;");

        std::cout << "Data copied from disk table to in-memory table." << std::endl;

        diskDb.exec("INSERT INTO memory.users (name, age) VALUES ('Bob', 25);");

        // 事务保证
        std::cout << diskDb.exec("BEGIN TRANSACTION;") << std::endl;
        std::cout << diskDb.exec("DELETE FROM users;") << std::endl;
        std::cout << diskDb.exec("INSERT INTO users SELECT * FROM memory.users;") << std::endl;
        std::cout << diskDb.exec("COMMIT;") << std::endl;

        // 关闭附加的磁盘数据库
        diskDb.exec("DETACH memory;");
        std::cout << "Disk database detached." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "SQLite error: " << e.what() << std::endl;
    }

    // 读取数据库
    try {
        SQLite::Database diskDb(DB_NAME, SQLite::OPEN_READWRITE);

        // 从磁盘数据库读取数据
        SQLite::Statement query(diskDb, "SELECT * FROM users;");
        while (query.executeStep()) {
            printf("Id: %d, Name: %s, Age: %d\n", query.getColumn(0).getInt(), query.getColumn(1).getText(), query.getColumn(2).getInt());
        }
    } catch(const std::exception& e) {
        std::cerr << e.what() << '\n';
    }

    std::remove(DB_NAME);
}

int main()
{
    test_attach_memory_db();

    // open_nonexistent_db();

    // if (sqlite3_threadsafe()) {
    //     std::cout << "SQLite is thread-safe" << std::endl;
    // } else {
    //     std::cout << "SQLite is NOT thread-safe" << std::endl;
    // }

    // attach_memory_db();

    return 0;
}

