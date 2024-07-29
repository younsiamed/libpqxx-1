#include <iostream>
#include <pqxx/pqxx>

void createDatabase(const std::string& server_conninfo, const std::string& dbname) {
    try {
        pqxx::connection conn{ server_conninfo };
        pqxx::nontransaction txn{ conn };
        txn.exec0("CREATE DATABASE " + txn.esc(dbname));
        std::cout << "Database created successfully." << std::endl;
    }
    catch (const pqxx::sql_error& e) {
        // Ignore the error if the database already exists
        if (std::string(e.what()).find("already exists") == std::string::npos) {
            std::cerr << "Database creation failed: " << e.what() << std::endl;
            throw;
        }
        else {
            std::cout << "Database already exists." << std::endl;
        }
    }
}

class ClientManager {
public:
    ClientManager(const std::string& conninfo) : conn(conninfo) {
        if (!conn.is_open()) {
            throw std::runtime_error("Cannot open database");
        }
    }

    void createTables() {
        pqxx::work txn{ conn };
        txn.exec("CREATE TABLE IF NOT EXISTS clients ("
            "id SERIAL PRIMARY KEY, "
            "first_name VARCHAR(50), "
            "last_name VARCHAR(50), "
            "email VARCHAR(100));");
        txn.exec("CREATE TABLE IF NOT EXISTS phones ("
            "id SERIAL PRIMARY KEY, "
            "client_id INTEGER REFERENCES clients(id) ON DELETE CASCADE, "
            "phone_number VARCHAR(20));");
        txn.commit();
        std::cout << "Tables created successfully." << std::endl;
    }

    int addClient(const std::string& first_name, const std::string& last_name, const std::string& email) {
        pqxx::work txn{ conn };
        pqxx::result res = txn.exec_params(
            "INSERT INTO clients (first_name, last_name, email) VALUES ($1, $2, $3) RETURNING id;",
            first_name, last_name, email);
        int client_id = res[0]["id"].as<int>();
        txn.commit();
        std::cout << "Client added successfully. ID: " << client_id << std::endl;
        return client_id;
    }

    void addPhone(int client_id, const std::string& phone_number) {
        pqxx::work txn{ conn };
        txn.exec_params("INSERT INTO phones (client_id, phone_number) VALUES ($1, $2);",
            client_id, phone_number);
        txn.commit();
        std::cout << "Phone added successfully." << std::endl;
    }

    void updateClient(int client_id, const std::string& first_name, const std::string& last_name, const std::string& email) {
        pqxx::work txn{ conn };
        txn.exec_params("UPDATE clients SET first_name = $1, last_name = $2, email = $3 WHERE id = $4;",
            first_name, last_name, email, client_id);
        txn.commit();
        std::cout << "Client updated successfully." << std::endl;
    }

    void deletePhone(int phone_id) {
        pqxx::work txn{ conn };
        txn.exec_params("DELETE FROM phones WHERE id = $1;", phone_id);
        txn.commit();
        std::cout << "Phone deleted successfully." << std::endl;
    }

    void deleteClient(int client_id) {
        pqxx::work txn{ conn };
        txn.exec_params("DELETE FROM clients WHERE id = $1;", client_id);
        txn.commit();
        std::cout << "Client deleted successfully." << std::endl;
    }

    void resetClientSequence() {
        pqxx::work txn{ conn };
        txn.exec("SELECT setval('clients_id_seq', 1, false);");
        txn.commit();
        std::cout << "Client sequence reset successfully." << std::endl;
    }

    void findClient(const std::string& query) {
        pqxx::work txn{ conn };
        pqxx::result res = txn.exec_params(
            "SELECT c.id, c.first_name, c.last_name, c.email, p.phone_number "
            "FROM clients c "
            "LEFT JOIN phones p ON c.id = p.client_id "
            "WHERE c.first_name ILIKE $1 "
            "OR c.last_name ILIKE $1 "
            "OR c.email ILIKE $1 "
            "OR p.phone_number ILIKE $1;", "%" + query + "%");

        for (const auto& row : res) {
            std::cout << "ID: " << row["id"].as<int>() << " | "
                << "First Name: " << row["first_name"].c_str() << " | "
                << "Last Name: " << row["last_name"].c_str() << " | "
                << "Email: " << row["email"].c_str() << " | "
                << "Phone: " << row["phone_number"].c_str() << std::endl;
        }
    }

private:
    pqxx::connection conn;
};

int main() {
    const std::string server_conninfo = "user=postgres password=1234 host=localhost port=5432";
    const std::string dbname = "yourdbname";
    const std::string db_conninfo = server_conninfo + " dbname=" + dbname;

    try {
        createDatabase(server_conninfo, dbname);
        ClientManager manager(db_conninfo);

        manager.createTables();

        int client_id = manager.addClient("John", "Doe", "john.doe@example.com");
        manager.addPhone(client_id, "+1234567890");
        manager.updateClient(client_id, "Johnny", "Doe", "johnny.doe@example.com");
        manager.findClient("Johnny");
        manager.deletePhone(client_id);
        manager.deleteClient(client_id);
        manager.resetClientSequence();

    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    return 0;
}
