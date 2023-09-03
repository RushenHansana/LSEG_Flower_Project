#include <fstream>
#include <iostream>
#include <vector>
#include <algorithm>
#include <string>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <thread>
#include <mutex>
#include <atomic> 
using namespace std;

string line,CLID,Instr,Side,Quantity,Price;


// Define a class named 'Buy_Sell'
class Buy_Sell {
public:
    // Object variables (properties)
    string OrderID;
    string CLID;
    string Instr;
    int Side;
    int Exec_status;
    int Quantity;
    double Price;
    Buy_Sell(string CLID,string Instr, int Side, int Quantity, double Price) : CLID(CLID),Instr(Instr),Side(Side),Quantity(Quantity),Price(Price){}
};

///////////////globaly defined orderbooks//////////////////////////////////
vector<Buy_Sell> Buy_Rose,Buy_Lavender,Buy_Lotus,Buy_Tulip,Buy_Orchid;
vector<Buy_Sell> Sell_Rose,Sell_Lavender,Sell_Lotus,Sell_Tulip,Sell_Orchid;
mutex timeMutex;
string currentTime;
std::atomic<bool> shouldStop(false);
//////////////for orderbook sorting////////////////////
bool compareByPriceA(const Buy_Sell &a, const Buy_Sell &b) {
    return a.Price < b.Price;
}
bool compareByPriceD(const Buy_Sell &a, const Buy_Sell &b) {
    return a.Price > b.Price;
}

////////////////////to get transaction time/////////////////////////////////////////
void get_current_time() {
    while (!shouldStop){
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::tm *tm = std::localtime(&now_c);
    std::stringstream ss;
    ss << std::put_time(tm, "%Y%m%d-%H%M%S") << '.' << std::setfill('0') << std::setw(3) << now_ms.count();
    std::lock_guard<std::mutex> lock(timeMutex);
    currentTime = ss.str();
    }
}

/////////////////exec status to word/////////////////////////
string exec_st(Buy_Sell order) {
    if (order.Exec_status == 0){
        return "New";
    }
    else if (order.Exec_status == 1){
        return "Rejected";
    }
    else if (order.Exec_status == 2){
       return "Fill";
    }
    else{
        return "Pfill";
    }
}

///////////////////////////////check the validity of the order///////////////////////////////////////

bool validateAndWriteToFile(ofstream& file, const string& OrderID, const string& CLID,const string& Instr,
                             const string& Side, const string& Quantity,const string& Price) {
    string current_time;
    lock_guard<std::mutex> lock(timeMutex);
    current_time = currentTime;
    if (CLID.length() < 1 || Instr.length() < 1 || Side.length() < 1 || Quantity.length() < 1 || Price.length() < 1) {
        file << OrderID << "," << CLID << "," << Instr << "," << Side << "," << "Rejected" << "," << Quantity << ","
             << Price << "," <<"does not contain a required field" <<"," << current_time<<"\n";
        return false;
    } else if (!(Instr == "Rose" || Instr == "Lavender" || Instr == "Lotus" || Instr == "Tulip" || Instr == "Orchid")) {
        file << OrderID << "," << CLID << "," << Instr << "," << Side << "," << "Rejected" << "," << Quantity << ","
             << Price << "," << "invalid Instrument"<<"," <<current_time << "\n";
        return false;
    } else if (!(Side == "1" || Side == "2")) {
        file << OrderID << "," << CLID << "," << Instr << "," << Side << "," << "Rejected" << "," << Quantity << ","
             << Price << "," << "invalid side"<<"," << current_time << "\n";
        return false;
    } else if (std::stod(Price) <= 0) {
        file << OrderID << "," << CLID << "," << Instr << "," << Side << "," << "Rejected" << "," << Quantity << ","
             << Price << "," << "price is not greater than 0"<<"," << current_time << "\n";
        return false;
    } else if (!(std::stoi(Quantity) % 10 == 0)) {
        file << OrderID << "," << CLID << "," << Instr << "," << Side << "," << "Rejected" << "," << Quantity << ","
             << Price << "," << "quantity is not a multiple of 10" <<"," << current_time<< "\n";
        return false;
    } else if (!(std::stoi(Quantity) <= 1000 && 10 <= std::stoi(Quantity))) {
        file << OrderID << "," << CLID << "," << Instr << "," << Side << "," << "Rejected" << "," << Quantity << ","
             << Price << "," << "quantity is outside the range"<<"," << current_time << "\n";
        return false;
    }

    // If all checks passed
    return true;
}

//////////////////////////process the Buy orders/////////////////////////////////////////////////////

void processOrdersBuy(vector<Buy_Sell> &Buy, vector<Buy_Sell> &Sell, Buy_Sell &order1, ofstream &file) {
    int neworder = 1;
    string current_time;
    lock_guard<std::mutex> lock(timeMutex);
    current_time = currentTime;

    while ((Sell.size() > 0) && (Sell[0].Price <= order1.Price) ) {
        neworder = 0;
        if (Sell[0].Quantity == order1.Quantity) {
            Sell[0].Exec_status = 2;
            order1.Exec_status = 2;
            file << order1.OrderID << "," << order1.CLID << "," << order1.Instr << "," << order1.Side << ","
                 << order1.Exec_status << "," << order1.Quantity << "," << Sell[0].Price<<",," <<current_time << "\n"
                 << Sell[0].OrderID << "," << Sell[0].CLID << "," << Sell[0].Instr << "," << Sell[0].Side << ","
                 << Sell[0].Exec_status << "," << Sell[0].Quantity << "," << Sell[0].Price <<",," << current_time<< "\n";
            Sell.erase(Sell.begin());
            break;
        } else if (Sell[0].Quantity > order1.Quantity) {
            Sell[0].Exec_status = 3;
            order1.Exec_status = 2;
            file << order1.OrderID << "," << order1.CLID << "," << order1.Instr << "," << order1.Side << ","
                 << order1.Exec_status << "," << order1.Quantity << "," << Sell[0].Price <<",," << current_time<< "\n";
            Sell[0].Quantity = Sell[0].Quantity - order1.Quantity;
            file << Sell[0].OrderID << "," << Sell[0].CLID << "," << Sell[0].Instr << "," << Sell[0].Side << ","
                 << Sell[0].Exec_status << "," << order1.Quantity << "," << Sell[0].Price <<",," << current_time<< "\n";
            break;
        } else if (Sell[0].Quantity < order1.Quantity) {
            Sell[0].Exec_status = 2;
            order1.Exec_status = 3;
            order1.Quantity = order1.Quantity - Sell[0].Quantity;
            file << order1.OrderID << "," << order1.CLID << "," << order1.Instr << "," << order1.Side << ","
                 << order1.Exec_status << "," << Sell[0].Quantity << "," << Sell[0].Price<<",," << current_time<< "\n";
            file << Sell[0].OrderID << "," << Sell[0].CLID << "," << Sell[0].Instr << "," << Sell[0].Side << ","
                 << Sell[0].Exec_status << "," << Sell[0].Quantity << "," << Sell[0].Price <<",," << current_time<< "\n";
            Sell.erase(Sell.begin());
        }
    }

    if (neworder == 1) {
        order1.Exec_status = 0;
        file << order1.OrderID << "," << order1.CLID << "," << order1.Instr << "," << order1.Side << ","
             << order1.Exec_status << "," << order1.Quantity << "," << order1.Price<<",," << current_time << "\n";
        Buy.push_back(order1);
        sort(Buy.begin(), Buy.end(), compareByPriceD);
    }

    if (order1.Exec_status == 3) {
        Buy.push_back(order1);
        sort(Buy.begin(), Buy.end(), compareByPriceD);
    }
}

////////////////////////////////process the sell orders////////////////////////////////////////////

void processOrdersSell(std::vector<Buy_Sell> &Buy, std::vector<Buy_Sell> &Sell, Buy_Sell &order1, std::ofstream &file) {
    int neworder = 1;
    string current_time;
    lock_guard<std::mutex> lock(timeMutex);
    current_time = currentTime;

    while ((Buy.size() > 0) && (Buy[0].Price >= order1.Price) ) {
        neworder = 0;

        if (Buy[0].Quantity == order1.Quantity) {
            Buy[0].Exec_status = 2;
            order1.Exec_status = 2;
            file << order1.OrderID << "," << order1.CLID << "," << order1.Instr << "," << order1.Side << ","
                 << order1.Exec_status << "," << order1.Quantity << "," << Buy[0].Price <<",," << current_time << "\n"
                 << Buy[0].OrderID << "," << Buy[0].CLID << "," << Buy[0].Instr << "," << Buy[0].Side << ","
                 << Buy[0].Exec_status << "," << Buy[0].Quantity << "," << Buy[0].Price <<",," << current_time<< "\n";
            Buy.erase(Buy.begin());
            break;
        } else if (Buy[0].Quantity > order1.Quantity) {
            Buy[0].Exec_status = 3;
            order1.Exec_status = 2;
            file << order1.OrderID << "," << order1.CLID << "," << order1.Instr << "," << order1.Side << ","
                 << order1.Exec_status << "," << order1.Quantity << "," << Buy[0].Price <<",," << current_time<< "\n";
            Buy[0].Quantity = Buy[0].Quantity - order1.Quantity;
            file << Buy[0].OrderID << "," << Buy[0].CLID << "," << Buy[0].Instr << "," << Buy[0].Side << ","
                 << Buy[0].Exec_status << "," << order1.Quantity << "," << Buy[0].Price<<",," << current_time << "\n";
            break;
        } else if (Buy[0].Quantity < order1.Quantity) {
            Buy[0].Exec_status = 2;
            order1.Exec_status = 3;
            order1.Quantity = order1.Quantity - Buy[0].Quantity;
            file << order1.OrderID << "," << order1.CLID << "," << order1.Instr << "," << order1.Side << ","
                 << order1.Exec_status << "," << Buy[0].Quantity << "," << Buy[0].Price<<",," << current_time << "\n";
            file << Buy[0].OrderID << "," << Buy[0].CLID << "," << Buy[0].Instr << "," << Buy[0].Side << ","
                 << Buy[0].Exec_status << "," << Buy[0].Quantity << "," << Buy[0].Price<<",," << current_time<< "\n";
            Buy.erase(Buy.begin());
        }
    }

    if (neworder == 1) {
        order1.Exec_status = 0;
        file << order1.OrderID << "," << order1.CLID << "," << order1.Instr << "," << order1.Side << ","
             << order1.Exec_status << "," << order1.Quantity << "," << order1.Price <<",," << current_time<< "\n";
        Sell.push_back(order1);
        sort(Sell.begin(), Sell.end(), compareByPriceA);
    }

    if (order1.Exec_status == 3) {
        Sell.push_back(order1);
        sort(Sell.begin(), Sell.end(), compareByPriceA);
    }
}



int main() {
    thread time_thread(get_current_time);
    this_thread::sleep_for(std::chrono::seconds(1));
    // Create an input file stream object
    ifstream fin;

    // Specify the path of the CSV file
    string filePath = "order (2).csv";

    // Open the CSV file
    fin.open(filePath);

    // Check if the file was opened successfully
    if (!fin.is_open()) {
        cout << "File Not Found in Folder." << endl;
        return 1;
    }
    // Create a new file named "execution_rep.csv" for writing
    ofstream file("execution_rep.csv");
    if (!file.is_open()) {
        cerr << "Error creating file." << endl;
        return 1;
    }
    file << "Order Id , " << "Cl. Ord. ID , " << "Instrument , " << "Side , " << "Exec Status , " << "Quantity , " << "Price , " << "Reason , " << "Transaction Time " << endl;
    int i = -1;
    while (fin.good()) {
        i++;

        getline(fin, CLID, ',');
        getline(fin, Instr, ',');
        getline(fin, Side, ',');
        getline(fin, Quantity, ',');
        getline(fin, Price, '\n');
        string OrderID = "ord"+to_string(i);
        //int order0 = 0;
        if (i==0){//do not read first line of order.csv//
            continue;
        }
        //////////check validity of order///////////////////
        if (!(validateAndWriteToFile(file, OrderID, CLID, Instr, Side, Quantity, Price))) {
            continue;
        }

        Buy_Sell order1(CLID,Instr,stoi(Side),stoi(Quantity),stod(Price));
        
        
        
        ///////////////////////////////buy side/////////////////////////////////

        if (stoi(Side)==1){
            order1.OrderID = "ord"+to_string(i);
            if (Instr == "Rose"){
                processOrdersBuy(Buy_Rose,Sell_Rose, order1,file);
            }
            else if(Instr == "Lavender"){
                processOrdersBuy(Buy_Lavender,Sell_Lavender, order1,file);
            }
            else if(Instr == "Lotus"){
                processOrdersBuy(Buy_Lotus,Sell_Lotus, order1,file);
            }
            else if(Instr == "Tulip"){
                processOrdersBuy(Buy_Tulip,Sell_Tulip, order1,file);
            }
            else {
                processOrdersBuy(Buy_Orchid,Sell_Orchid, order1,file);
            }


        }

        ///////////////////////////////////Sell Side///////////////////////////////////////////////

        else{
            order1.OrderID = "ord"+to_string(i);
            if (Instr == "Rose"){
                processOrdersSell(Buy_Rose,Sell_Rose, order1,file);
            }
            else if(Instr == "Lavender"){
                processOrdersSell(Buy_Lavender,Sell_Lavender, order1,file);
            }
            else if(Instr == "Lotus"){
                processOrdersSell(Buy_Lotus,Sell_Lotus, order1,file);
            }
            else if(Instr == "Tulip"){
                processOrdersSell(Buy_Tulip,Sell_Tulip, order1,file);
            }
            else {
                processOrdersSell(Buy_Orchid,Sell_Orchid, order1,file);
            }
        
    }
    }
    shouldStop = true;
    time_thread.join();
    // Close the file
    file.flush();
    file.close();
    return 0;
}
