#include <crow.h>
#include <iostream>
#include <mysql/mysql.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <httplib.h>
#include <jwt-cpp/jwt.h>

using json [[maybe_unused]] = nlohmann::json;

bool connectToDatabase(MYSQL *&conn) {
    conn = mysql_init(nullptr);
    if (!conn) {
        std::cout << "Connection object initialization failed" << std::endl;
        return false;
    }

    if (!mysql_real_connect(conn, "0.0.0.0", "root", "root", "sono", 3306, nullptr, 0)) {
        std::cout << "Connection failed: " << mysql_error(conn) << std::endl;
        return false;
    }

    std::cout << "Connection success" << std::endl;
    return true;
}

bool authenticateAdmin(const std::string& admin, const std::string& passwrd, MYSQL* conn) {
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt) {
        std::cerr << "Could not initialize statement: " << mysql_error(conn) << std::endl;
        return false;
    }

    const char* query = "SELECT COUNT(*) FROM authentification WHERE admin = ? AND passwrd = ?";
    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        std::cerr << "Could not prepare statement: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND bind[2];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = const_cast<char*>(admin.c_str());
    bind[0].buffer_length = admin.size();

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = const_cast<char*>(passwrd.c_str());
    bind[1].buffer_length = passwrd.size();

    if (mysql_stmt_bind_param(stmt, bind)) {
        std::cerr << "Could not bind parameters: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_execute(stmt)) {
        std::cerr << "Could not execute statement: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND result_bind[1];
    memset(result_bind, 0, sizeof(result_bind));

    int count;
    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = &count;
    result_bind[0].buffer_length = sizeof(count);

    if (mysql_stmt_bind_result(stmt, result_bind)) {
        std::cerr << "Could not bind result: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_fetch(stmt)) {
        std::cerr << "Could not fetch result: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return false;
    }

    mysql_stmt_close(stmt);
    return count > 0;
}

bool authenticateClient(const std::string& client, const std::string& passwrd, MYSQL* conn) {
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt) {
        std::cerr << "Could not initialize statement: " << mysql_error(conn) << std::endl;
        return false;
    }

    const char* query = "SELECT COUNT(*) FROM clients WHERE client = ? AND passwrd = ?";
    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        std::cerr << "Could not prepare statement: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND bind[2];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = const_cast<char*>(client.c_str());
    bind[0].buffer_length = client.size();

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = const_cast<char*>(passwrd.c_str());
    bind[1].buffer_length = passwrd.size();

    if (mysql_stmt_bind_param(stmt, bind)) {
        std::cerr << "Could not bind parameters: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_execute(stmt)) {
        std::cerr << "Could not execute statement: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return false;
    }

    MYSQL_BIND result_bind[1];
    memset(result_bind, 0, sizeof(result_bind));

    int count;
    result_bind[0].buffer_type = MYSQL_TYPE_LONG;
    result_bind[0].buffer = &count;
    result_bind[0].buffer_length = sizeof(count);

    if (mysql_stmt_bind_result(stmt, result_bind)) {
        std::cerr << "Could not bind result: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return false;
    }

    if (mysql_stmt_fetch(stmt)) {
        std::cerr << "Could not fetch result: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return false;
    }

    mysql_stmt_close(stmt);
    return count > 0;
}

std::string generateJWTToken(const std::string& userId) {
    auto token = jwt::create()
            .set_issuer("api_rest_test")
            .set_type("JWT")
            .set_payload_claim("client1", jwt::claim(userId))
            .set_issued_at(std::chrono::system_clock::now())
            .set_expires_at(std::chrono::system_clock::now() + std::chrono::minutes {1})
            .sign(jwt::algorithm::hs256{"client"});

    return token;
}

bool validateJWTToken(const std::string& token, std::string& errorMessage) {
    try {
        auto decoded_token = jwt::decode(token);
        auto verifier = jwt::verify()
                .allow_algorithm(jwt::algorithm::hs256{"client"})
                .with_issuer("api_rest_test");

        verifier.verify(decoded_token);

        if (decoded_token.has_expires_at() && decoded_token.get_expires_at() <= std::chrono::system_clock::now()) {
            errorMessage = "Token expired";
            return false;
        }

        return true;
    } catch (const jwt::error::token_verification_exception& e) {
        errorMessage = "Token verification failed: " + std::string(e.what());
        return false;
    }
}


crow::response handleLogin(const crow::request &req, MYSQL *conn) {
    if (req.method == crow::HTTPMethod::Post) {
        std::string body = req.body;

        std::string admin, passwrd;
        size_t pos = body.find("admin=");
        if (pos != std::string::npos) {
            pos += strlen("admin=");
            size_t end_pos = body.find('&', pos);
            if (end_pos != std::string::npos) {
                admin = httplib::detail::decode_url(body.substr(pos, end_pos - pos), true);
            }
        }

        pos = body.find("passwrd=");
        if (pos != std::string::npos) {
            pos += strlen("passwrd=");
            passwrd = httplib::detail::decode_url(body.substr(pos), true);
        }

        if (admin.empty() || passwrd.empty()) {
            return {400, "Empty fields in form"};
        }

        if (authenticateAdmin(admin, passwrd, conn)) {
            std::string token = generateJWTToken(admin);
            crow::response res;
            res.set_header("Set-Cookie", "jwt=" + token + "; HttpOnly; Path=/");
            res.set_header("Location", "/data");
            res.code = 302;
            return res;
        } else {
            return {401, "Authentication failed"};
        }
    } else {
        return {405, "Method Not Allowed"};
    }
}

crow::response handleClientLogin(const crow::request &req, MYSQL *conn) {
    if (req.method == crow::HTTPMethod::Post) {
        std::string body = req.body;

        std::string client, passwrd;
        size_t pos = body.find("client=");
        if (pos != std::string::npos) {
            pos += strlen("client=");
            size_t end_pos = body.find('&', pos);
            if (end_pos != std::string::npos) {
                client = httplib::detail::decode_url(body.substr(pos, end_pos - pos), true);
            }
        }

        pos = body.find("passwrd=");
        if (pos != std::string::npos) {
            pos += strlen("passwrd=");
            passwrd = httplib::detail::decode_url(body.substr(pos), true);
        }

        if (client.empty() || passwrd.empty()) {
            return {400, "Empty fields in form"};
        }

        if (authenticateClient(client, passwrd, conn)) {
            std::string token = generateJWTToken(client);
            crow::response res;
            res.set_header("Set-Cookie", "jwt=" + token + "; HttpOnly; Path=/");
            res.set_header("Location", "/data");
            res.code = 302;
            return res;
        } else {
            return {401, "Authentication failed"};
        }
    } else {
        return {405, "Method Not Allowed"};
    }
}

std::string readFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filePath);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

crow::response handleData(const crow::request &req, MYSQL *conn) {
    if (req.method == crow::HTTPMethod::Get) {
        auto cookie_header = req.get_header_value("Cookie");
        std::string token;
        size_t pos = cookie_header.find("jwt=");
        if (pos != std::string::npos) {
            pos += strlen("jwt=");
            size_t end_pos = cookie_header.find(';', pos);
            if (end_pos == std::string::npos) {
                end_pos = cookie_header.length();
            }
            token = cookie_header.substr(pos, end_pos - pos);
        }

        if (token.empty()) {
            return {401, "Missing JWT token"};
        }

        std::string errorMessage;
        if (!validateJWTToken(token, errorMessage)) {
            if (errorMessage == "Token expired") {
                return {440, "Session expired"}; // 440 Login Timeout
            } else {
                return {401, "Invalid JWT token"};
            }
        }

        std::ostringstream oss;
        if (mysql_query(conn, "SELECT * FROM sono")) {
            oss << "Error reading table: " << mysql_error(conn);
            return {501, oss.str()};
        } else {
            MYSQL_RES *result = mysql_store_result(conn);
            if (result) {
                std::string htmlTemplate;
                try {
                    htmlTemplate = readFile("data.html");
                } catch (const std::exception& e) {
                    return {502, e.what()};
                }

                std::ostringstream dataStream;
                MYSQL_ROW row;
                while ((row = mysql_fetch_row(result))) {
                    dataStream << "<tr>";
                    for (unsigned int i = 0; i < mysql_num_fields(result); i++) {
                        dataStream << "<td>" << (row[i] ? row[i] : "NULL") << "</td>";
                    }
                    dataStream << "</tr>";
                }
                mysql_free_result(result);

                std::string dataString = dataStream.str();
                pos = htmlTemplate.find("{DATA}");
                if (pos != std::string::npos) {
                    htmlTemplate.replace(pos, 6, dataString);
                } else {
                    return {503, "Template placeholder not found"};
                }

                return {200, htmlTemplate};
            } else {
                oss << "Error reading table: " << mysql_error(conn);
                return {504, oss.str()};
            }
        }
    } else {
        return {405, "Method Not Allowed"};
    }
}

crow::response handleDataCreate(const crow::request &req, MYSQL *conn) {
    if (req.method == crow::HTTPMethod::Post) {
        auto json = crow::json::load(req.body);
        if (!json)
            return {400, "Invalid JSON"};

        if (!json.has("id") || !json.has("humidity") || !json.has("noise"))
            return {400, "Missing fields in JSON"};

        int id = json["id"].i();
        std::string humidity = json["humidity"].s();
        int noise = json["noise"].i();

        MYSQL_STMT* stmt = mysql_stmt_init(conn);
        if (!stmt) {
            return {500, "Could not initialize statement: " + std::string(mysql_error(conn))};
        }

        const char* query = "INSERT INTO sono (id, humidity, noise) VALUES (?, ?, ?)";
        if (mysql_stmt_prepare(stmt, query, strlen(query))) {
            std::string error = "Could not prepare statement: " + std::string(mysql_stmt_error(stmt));
            mysql_stmt_close(stmt);
            return {500, error};
        }

        MYSQL_BIND bind[3];
        memset(bind, 0, sizeof(bind));

        bind[0].buffer_type = MYSQL_TYPE_LONG;
        bind[0].buffer = &id;
        bind[0].buffer_length = sizeof(id);

        bind[1].buffer_type = MYSQL_TYPE_STRING;
        bind[1].buffer = const_cast<char*>(humidity.c_str());
        bind[1].buffer_length = humidity.size();

        bind[2].buffer_type = MYSQL_TYPE_LONG;
        bind[2].buffer = &noise;
        bind[2].buffer_length = sizeof(noise);

        if (mysql_stmt_bind_param(stmt, bind)) {
            std::string error = "Could not bind parameters: " + std::string(mysql_stmt_error(stmt));
            mysql_stmt_close(stmt);
            return {500, error};
        }

        if (mysql_stmt_execute(stmt)) {
            std::string error = "Could not execute statement: " + std::string(mysql_stmt_error(stmt));
            mysql_stmt_close(stmt);
            return {500, error};
        }

        mysql_stmt_close(stmt);
        return {201, "Data created successfully"};
    } else {
        return {405, "Method Not Allowed"};
    }
}

crow::response handleDataUpdate(const crow::request &req, int id, MYSQL *conn) {
    if (req.method == crow::HTTPMethod::Put) {
        auto json = crow::json::load(req.body);
        if (!json)
            return {400, "Invalid JSON"};

        if (!json.has("humidity") || !json.has("noise"))
            return {400, "Missing fields in JSON"};

        std::string humidity = json["humidity"].s();
        int noise = json["noise"].i();

        MYSQL_STMT* stmt = mysql_stmt_init(conn);
        if (!stmt) {
            return {500, "Could not initialize statement: " + std::string(mysql_error(conn))};
        }

        const char* query = "UPDATE sono SET humidity = ?, noise = ? WHERE id = ?";
        if (mysql_stmt_prepare(stmt, query, strlen(query))) {
            std::string error = "Could not prepare statement: " + std::string(mysql_stmt_error(stmt));
            mysql_stmt_close(stmt);
            return {500, error};
        }

        MYSQL_BIND bind[3];
        memset(bind, 0, sizeof(bind));

        bind[0].buffer_type = MYSQL_TYPE_STRING;
        bind[0].buffer = const_cast<char*>(humidity.c_str());
        bind[0].buffer_length = humidity.size();

        bind[1].buffer_type = MYSQL_TYPE_LONG;
        bind[1].buffer = &noise;
        bind[1].buffer_length = sizeof(noise);

        bind[2].buffer_type = MYSQL_TYPE_LONG;
        bind[2].buffer = &id;
        bind[2].buffer_length = sizeof(id);

        if (mysql_stmt_bind_param(stmt, bind)) {
            std::string error = "Could not bind parameters: " + std::string(mysql_stmt_error(stmt));
            mysql_stmt_close(stmt);
            return {500, error};
        }

        if (mysql_stmt_execute(stmt)) {
            std::string error = "Could not execute statement: " + std::string(mysql_stmt_error(stmt));
            mysql_stmt_close(stmt);
            return {500, error};
        }

        mysql_stmt_close(stmt);
        return {200, "Data updated successfully"};
    } else {
        return {405, "Method Not Allowed"};
    }
}

crow::response handleDataDelete(int id, MYSQL *conn) {
    MYSQL_STMT* stmt = mysql_stmt_init(conn);
    if (!stmt) {
        return {500, "Could not initialize statement: " + std::string(mysql_error(conn))};
    }

    const char* query = "DELETE FROM sono WHERE id = ?";
    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        std::string error = "Could not prepare statement: " + std::string(mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return {500, error};
    }

    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = &id;
    bind[0].buffer_length = sizeof(id);

    if (mysql_stmt_bind_param(stmt, bind)) {
        std::string error = "Could not bind parameters: " + std::string(mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return {500, error};
    }

    if (mysql_stmt_execute(stmt)) {
        std::string error = "Could not execute statement: " + std::string(mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return {500, error};
    }

    mysql_stmt_close(stmt);
    return {200, "Data deleted successfully"};
}

crow::response handlelogout([[maybe_unused]] const crow::request &req, [[maybe_unused]] MYSQL *conn) {
    crow::response res(302);
    res.set_header("Location", "/");
    res.set_header("Set-Cookie", "jwt=; HttpOnly; Path=/; Max-Age=0"); // Clear the cookie
    return res;
}

int main() {
    MYSQL *conn;
    if (!connectToDatabase(conn)) {
        return 1;
    }

    crow::SimpleApp app;

    CROW_ROUTE(app, "/").methods("GET"_method)([]() {
        std::ifstream ifs("login.html");
        std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
        return crow::response(200, content);
    });

    CROW_ROUTE(app, "/login").methods("POST"_method)([&conn](const crow::request &req) {
        return handleLogin(req, conn);
    });

    CROW_ROUTE(app, "/client/login").methods("POST"_method)([&conn](const crow::request &req) {
        return handleClientLogin(req, conn);
    });

    CROW_ROUTE(app, "/client/login").methods("GET"_method)([]() {
        std::ifstream ifs("client_login.html");
        std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
        return crow::response(200, content);
    });

    CROW_ROUTE(app, "/data").methods("GET"_method)([&conn](const crow::request &req) {
        return handleData(req, conn);
    });

    CROW_ROUTE(app, "/data/create").methods("POST"_method)([&conn](const crow::request &req) {
        return handleDataCreate(req, conn);
    });

    CROW_ROUTE(app, "/data/update/<int>").methods("PUT"_method)([&conn](const crow::request &req, int id) {
        return handleDataUpdate(req, id, conn);
    });

    CROW_ROUTE(app, "/data/delete/<int>").methods("DELETE"_method)([&conn](int id) {
        return handleDataDelete(id, conn);
    });

    CROW_ROUTE(app, "/logout").methods("GET"_method)([&conn](const crow::request &req) {
        return handlelogout(req, conn);
    });

    app.port(8080).multithreaded().run();

    mysql_close(conn);
    return 0;
}
