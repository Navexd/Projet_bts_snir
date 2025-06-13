#include <crow.h>
#include <iostream>
#include <mariadb/mysql.h>
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

    if (!mysql_real_connect(conn, "10.194.177.90", "pi", "raspi", "sono", 3306, nullptr, 0)) {
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
            .set_expires_at(std::chrono::system_clock::now() + std::chrono::minutes {5})
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
            res.set_header("Location", "/datad");  // Redirect to /datad for admin
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
            res.set_header("Location", "/data");  // Redirect to /data for client
            res.code = 302;
            return res;
        } else {
            return {401, "Authentication failed"};
        }
    } else {
        return {405, "Method Not Allowed"};
    }
}

std::string readFile(const std::string &path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Could not open file: " + path);
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
                    htmlTemplate = readFile("./data/data.html");
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

crow::response handleAdminData(const crow::request &req, MYSQL *conn) {
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
                    htmlTemplate = readFile("./data/datad.html");
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
        auto jsonPayload = json::parse(req.body);

        int id = jsonPayload["id"];
        int humidity = jsonPayload["humidity"];
        int noise = jsonPayload["noise"];

        std::string query = "INSERT INTO sono (id, humidity, noise) VALUES (" + std::to_string(id) + ", " + std::to_string(humidity) + ", " + std::to_string(noise) + ")";

        if (mysql_query(conn, query.c_str())) {
            return {500, mysql_error(conn)};
        }

        return {201, "Record created"};
    } else {
        return {405, "Method Not Allowed"};
    }
}


crow::response handleDataUpdate(const crow::request &req, int id, MYSQL *conn) {
    if (req.method == crow::HTTPMethod::Put) {
        auto jsonPayload = json::parse(req.body);

        int humidity = jsonPayload["humidity"];
        int noise = jsonPayload["noise"];

        std::string query = "UPDATE sono SET humidity = " + std::to_string(humidity) + ", noise = " + std::to_string(noise) + " WHERE id = " + std::to_string(id);

        if (mysql_query(conn, query.c_str())) {
            return {500, mysql_error(conn)};
        }

        return {200, "Record updated"};
    } else {
        return {405, "Method Not Allowed"};
    }
}

crow::response handleDataDelete(int id, MYSQL *conn) {
    if (mysql_query(conn, ("DELETE FROM sono WHERE id = " + std::to_string(id)).c_str())) {
        return {500, mysql_error(conn)};
    }

    return {200, "Record deleted"};
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
        std::ifstream ifs("client_login.html");
        std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
        return crow::response(200, content);
    });

    CROW_ROUTE(app, "/login").methods("POST"_method)([&conn](const crow::request &req) {
        return handleLogin(req, conn);
    });

    CROW_ROUTE(app, "/client/login").methods("POST"_method)([&conn](const crow::request &req) {
        return handleClientLogin(req, conn);
    });

    CROW_ROUTE(app, "/admin/login").methods("GET"_method)([]() {
        std::ifstream ifs("admin_login.html");
        std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
        return crow::response(200, content);
    });

    CROW_ROUTE(app, "/data").methods("GET"_method)([&conn](const crow::request &req) {
        return handleData(req, conn);
    });

    CROW_ROUTE(app, "/datad").methods("GET"_method)([&conn](const crow::request &req) {
        return handleAdminData(req, conn);
    });

    CROW_ROUTE(app, "/data/create").methods("POST"_method)([&conn](const crow::request &req) {
        return handleDataCreate(req, conn);
    });

    CROW_ROUTE(app, "/data/update/<int>").methods("PUT"_method)([&conn](const crow::request &req, int id) {
        return handleDataUpdate(req, id, conn);
    });


    CROW_ROUTE(app, "/data/<int>").methods("DELETE"_method)([&conn](int id) {
        return handleDataDelete(id, conn);
    });


    CROW_ROUTE(app, "/logs").methods("GET"_method)([]() {
        try {
            std::string logContent = readFile("logs.txt");
            std::string htmlContent = "<!DOCTYPE html><html lang=\"fr\"><head><meta charset=\"UTF-8\"><title>Server Logs</title></head><body><pre>" + logContent + "</pre></body></html>";
            return crow::response(200, htmlContent);
        } catch (const std::exception& e) {
            return crow::response(500, e.what());
        }
    });

    CROW_ROUTE(app, "/logout").methods("GET"_method)([&conn](const crow::request &req) {
        return handlelogout(req, conn);
    });

    app.port(80).multithreaded().run();

    mysql_close(conn);
    return 0;
}
