#include <iostream>
#include <libserialport.h>
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/prepared_statement.h>
#include <regex>
#include <string>

#define USB_PORT "/dev/ttyACM0"
#define BAUD_RATE 9600

void read_and_store_data(sql::Connection *con) {
    sp_port *port;
    sp_return result = sp_get_port_by_name(USB_PORT, &port);
    if (result != SP_OK) {
        std::cerr << "Error finding port: " << USB_PORT << std::endl;
        return;
    }

    result = sp_open(port, SP_MODE_READ);
    if (result != SP_OK) {
        std::cerr << "Error opening port: " << USB_PORT << std::endl;
        return;
    }

    result = sp_set_baudrate(port, BAUD_RATE);
    if (result != SP_OK) {
        std::cerr << "Error setting baud rate." << std::endl;
        sp_close(port);
        return;
    }

    char buffer[256];
    int bytes_read;
    std::string data;
    std::regex regex_pattern(R"((\d+\.\d+) dBA Humidité: (\d+\.\d+) %)");

    while (true) {
        bytes_read = sp_nonblocking_read(port, buffer, sizeof(buffer) - 1);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            data += buffer;

            std::smatch matches;
            while (std::regex_search(data, matches, regex_pattern)) {
                double noise_level = std::stod(matches[1].str());
                double humidity_level = std::stod(matches[2].str());

                // Insert data into the database
                try {
                    sql::PreparedStatement *pstmt;

                    // Insert both noise and humidity into the table
                    pstmt = con->prepareStatement("INSERT INTO sono (noise, humidity) VALUES (?, ?)");
                    pstmt->setDouble(1, noise_level);
                    pstmt->setDouble(2, humidity_level);
                    pstmt->execute();
                    delete pstmt;

                    std::cout << "Inserted: " << noise_level << " dBA, Humidité: " << humidity_level << " %" << std::endl;
                } catch (sql::SQLException &e) {
                    std::cerr << "Error inserting data: " << e.what() << std::endl;
                }

                // Erase the processed part of the data string
                data = matches.suffix().str();
            }
        }
    }

    sp_close(port);
}

int main() {
    try {
        sql::mysql::MySQL_Driver *driver;
        sql::Connection *con;

        driver = sql::mysql::get_mysql_driver_instance();
        con = driver->connect("tcp://10.194.177.90:3306", "pi", "raspi");
        con->setSchema("sono");

        read_and_store_data(con);

        delete con;
    } catch (sql::SQLException &e) {
        std::cerr << "MySQL error: " << e.what() << std::endl;
    }

    return 0;
}