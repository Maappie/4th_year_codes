#include <iostream>
using namespace std;

int main() {
    int num = 10;
    int* ptr = &num; // pointer pointing to num

    cout << "This is the memory address of num: " << &num << endl; 
    cout << "This is the value of variable num: " << num << endl;
    cout << "This is the ptr that holds the memory address of num: " << ptr << endl;
    cout << "This is the value inside the ptr address: " << *ptr << endl;

    // --- Null pointer example ---
    int* nullPtr = nullptr; // points to nothing
    cout << "\nNull pointer created." << endl;
    cout << "nullPtr value (address it holds): " << nullPtr << endl;

    if (nullPtr == nullptr) {
        cout << "nullPtr is pointing to NOTHING right now." << endl;
    }

    // Assigning nullPtr to point to num
    nullPtr = &num;
    cout << "\nAfter assigning, nullPtr now points to num." << endl;
    cout << "nullPtr value (address it holds): " << nullPtr << endl;
    cout << "Value at nullPtr: " << *nullPtr << endl;

    // Changing value through pointer
    *nullPtr = 20; // modifies num
    cout << "\nAfter changing *nullPtr to 20:" << endl;
    cout << "num: " << num << endl;
    cout << "*ptr: " << *ptr << " (same num)" << endl;
	cout << "value of num: " << num << endl;
	
	cout << "value of ptr that hold the address of num: " << ptr << endl;
	cout << "address of nullPtr: " << nullPtr << endl;
	cout << "address of num: " << &num << endl; 