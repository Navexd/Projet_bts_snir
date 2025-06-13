echo "Installation des library en cours..."
sudo apt install libmariadb-dev -y

echo "Installation de asio"
sudo apt install libasio-dev -y

echo "installation de crow"
git clone https://github.com/CrowCpp/Crow.git

echo "installation de nlohmann/json"
git clone https://github.com/nlohmann/json.git

echo "installation de httplib"
git clone https://github.com/yhirose/cpp-httplib.git

echo "installation de jwt-cpp"
git clone https://github.com/Thalhammer/jwt-cpp.git

echo "Compilation en cours..."
g++ -I./Crow/include -I/usr/include/mariadb/ -I./json/include -I./cpp-httplib -I./jwt-cpp/include -o serv main.cpp -lmariadb -lssl -lcrypto
echo "Compilation terminer !"
sleep 1
echo "Fermeture auto dans 5s"
sleep 1
echo "4..."
sleep 1
echo "3..."
sleep 1
echo "2..."
sleep 1
echo "1..."
sleep 1
echo "Fermeture."

# Le script se termine ici
exit 0
