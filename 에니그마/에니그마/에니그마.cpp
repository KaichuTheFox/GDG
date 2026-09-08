// ============================================================
//  Simple Enigma Machine Simulator (C++, Windows console)
//
//  - Type letters (Q~P, A~L, Z~M) to encrypt them
//  - LEFT / RIGHT arrow : select rotor (1st / 2nd / 3rd)  [manual]
//  - UP / DOWN arrow    : increase / decrease selected rotor [manual]
//  - SPACE              : word space, shown as '_' (rotors do NOT move)
//  - BACKSPACE          : delete last character; if it was a letter,
//                          rotors also step back to what they were
//                          before that letter was typed
//  - '='                : quit
//
//  Auto-stepping rule (notch-based, applied only when a LETTER
//  is typed -- the space bar does not move the rotors):
//      - rotor[2] (rightmost) advances by 1 every keypress
//      - the moment rotor[2] becomes 'W' (the notch letter),
//        rotor[1] (middle) also advances by 1 in that same step
//      - the moment rotor[1] becomes 'W', rotor[0] (leftmost)
//        also advances by 1 in that same step
//
//  Compile (Windows, g++/MinGW):
//      g++ -O2 -o enigma.exe enigma.cpp
// ============================================================

#include <iostream>
#include <string>
#include <vector>
#include <array>
#include <conio.h>
#include <windows.h>
#include <cctype>

using namespace std;

// ---- Standard Enigma I rotor wirings + Reflector B ----
const string ROTOR_WIRING[3] = {
    "EKMFLGDQVZNTOWYHXUSPAIBRCJ", // Rotor I   -> leftmost box
    "AJDKSIRUXBLHWTMCQGZNPYFVOE", // Rotor II  -> middle box
    "BDFHJLCPRTXVZNYEIWGAKMUSQO"  // Rotor III -> rightmost box
};
const string REFLECTOR = "YRUHQSLDPXNGOKMIEBFZCWVJAT"; // Reflector B

int rotorPos[3] = { 0, 0, 0 };  // A=0 ... Z=25   [0]=left [1]=middle [2]=right
int selected = 0;          // which rotor (0,1,2) is currently selected (for manual arrow control)

string typedText = "";
string cipherText = "";
char lastInput = 0;
char lastOutput = 0;

// history of rotor states BEFORE each typed character (letter or space),
// used to undo the auto-stepping when Backspace is pressed
vector<array<int, 3>> history;

const int COLOR_NORMAL = 7;
const int COLOR_HILITE = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY; // yellow

void setColor(int c) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, (WORD)c);
}

int mod26(int x) {
    return ((x % 26) + 26) % 26;
}

// pass a letter forward through a rotor (keyboard side -> reflector side)
int rotorForward(const string& wiring, int pos, int c) {
    int shifted = mod26(c + pos);
    int out = wiring[shifted] - 'A';
    return mod26(out - pos);
}

// pass a letter backward through a rotor (reflector side -> lamp side)
int rotorBackward(const string& wiring, int pos, int c) {
    int target = mod26(c + pos);
    char targetChar = (char)('A' + target);
    int idx = (int)wiring.find(targetChar);
    return mod26(idx - pos);
}

char encryptChar(char inputChar) {
    int c = toupper((unsigned char)inputChar) - 'A';

    // forward: right rotor -> middle rotor -> left rotor
    c = rotorForward(ROTOR_WIRING[2], rotorPos[2], c);
    c = rotorForward(ROTOR_WIRING[1], rotorPos[1], c);
    c = rotorForward(ROTOR_WIRING[0], rotorPos[0], c);

    // reflector
    c = REFLECTOR[c] - 'A';

    // backward: left rotor -> middle rotor -> right rotor
    c = rotorBackward(ROTOR_WIRING[0], rotorPos[0], c);
    c = rotorBackward(ROTOR_WIRING[1], rotorPos[1], c);
    c = rotorBackward(ROTOR_WIRING[2], rotorPos[2], c);

    return (char)('A' + c);
}

// notch letter: when a rotor reaches this position, the next rotor to its
// left also steps in the same keypress ('W' = 23rd letter of the alphabet)
const int NOTCH = 'W' - 'A';

// advance rotors one step (called once per typed character, before encrypting)
void stepRotors() {
    rotorPos[2] = mod26(rotorPos[2] + 1);
    if (rotorPos[2] == NOTCH) {             // rotor 3 just reached 'W'
        rotorPos[1] = mod26(rotorPos[1] + 1);
        if (rotorPos[1] == NOTCH) {         // rotor 2 just reached 'W'
            rotorPos[0] = mod26(rotorPos[0] + 1);
        }
    }
}

void printRow(const string& indent, const string& letters, char highlight) {
    cout << indent;
    for (size_t i = 0; i < letters.size(); i++) {
        char ch = letters[i];
        if (highlight != 0 && ch == highlight) {
            setColor(COLOR_HILITE);
            cout << ch;
            setColor(COLOR_NORMAL);
        }
        else {
            cout << ch;
        }
        if (i + 1 < letters.size()) cout << ' ';
    }
    cout << '\n';
}

void printKeyboard(char highlight) {
    printRow("", "QWERTYUIOP", highlight);
    printRow("  ", "ASDFGHJKL", highlight);
    printRow("    ", "ZXCVBNM", highlight);
}

void drawRotors() {
    cout << "|--|   |--|   |--|\n";
    for (int i = 0; i < 3; i++) {
        char letter = (char)('A' + rotorPos[i]);
        cout << "|";
        if (i == selected) {
            setColor(COLOR_HILITE);
            cout << letter;
            setColor(COLOR_NORMAL);
        }
        else {
            cout << letter;
        }
        cout << "|";
        if (i < 2) cout << "   ";
    }
    cout << "\n";
    cout << "|--|   |--|   |--|\n";

    // selection indicator arrow under the currently selected rotor
    string indicator(18, ' ');
    int pos = selected * 7 + 1;
    if (pos < (int)indicator.size()) indicator[pos] = '^';
    cout << indicator << "  (selected)\n";
}

void drawScreen() {
    system("cls");
    cout << "================= ENIGMA MACHINE (C++) =================\n";
    cout << "Arrow UP/DOWN    : change value of selected rotor (manual)\n";
    cout << "Arrow LEFT/RIGHT : select rotor (1st / 2nd / 3rd)\n";
    cout << "Letter keys      : type & encrypt a letter (rotors auto-step)\n";
    cout << "SPACE            : word space, shown as '_' (rotors don't move)\n";
    cout << "BACKSPACE        : delete last char, rotors step back if it was a letter\n";
    cout << "'='              : quit program\n";
    cout << "==========================================================\n\n";

    cout << "               ROTORS\n";
    drawRotors();
    cout << "\n";

    cout << "  KEYBOARD (input)\n";
    printKeyboard(lastInput);
    cout << "\n";

    cout << "  LAMPBOARD (output)\n";
    printKeyboard(lastOutput);
    cout << "\n";

    cout << "----------------------------------------------------------\n";
    cout << "Typed  : " << typedText << "\n";
    cout << "Cipher : " << cipherText << "\n";
    cout << "----------------------------------------------------------\n";
}

int main() {
    setColor(COLOR_NORMAL);
    drawScreen();

    while (true) {
        int ch = _getch();

        if (ch == '=') {
            break;
        }
        else if (ch == 8) {
            // Backspace: remove last char and undo the rotor step it caused
            // (only letters pushed a rotor-history entry; spaces did not)
            if (!typedText.empty()) {
                char removed = typedText.back();
                typedText.pop_back();
                cipherText.pop_back();

                if (removed != '_' && !history.empty()) {
                    array<int, 3> prev = history.back();
                    history.pop_back();
                    rotorPos[0] = prev[0];
                    rotorPos[1] = prev[1];
                    rotorPos[2] = prev[2];
                }

                if (!typedText.empty()) {
                    char t = typedText.back();
                    char c = cipherText.back();
                    lastInput = (t == '_') ? 0 : t;
                    lastOutput = (c == '_') ? 0 : c;
                }
                else {
                    lastInput = 0;
                    lastOutput = 0;
                }
            }
        }
        else if (ch == 0 || ch == 224) {
            // extended key (arrow keys etc.) -- manual rotor control, not recorded in history
            int ch2 = _getch();
            switch (ch2) {
            case 72: // UP
                rotorPos[selected] = mod26(rotorPos[selected] + 1);
                break;
            case 80: // DOWN
                rotorPos[selected] = mod26(rotorPos[selected] - 1);
                break;
            case 75: // LEFT
                selected = (selected - 1 + 3) % 3;
                break;
            case 77: // RIGHT
                selected = (selected + 1) % 3;
                break;
            default:
                break;
            }
        }
        else if (ch == ' ') {
            // word space: shown as '_', does NOT step the rotors
            typedText += '_';
            cipherText += '_';
            lastInput = 0;
            lastOutput = 0;
        }
        else if (isalpha((unsigned char)ch)) {
            char upper = (char)toupper(ch);

            array<int, 3> snapshot = { rotorPos[0], rotorPos[1], rotorPos[2] };
            history.push_back(snapshot);
            stepRotors();

            char enc = encryptChar(upper);
            lastInput = upper;
            lastOutput = enc;
            typedText += upper;
            cipherText += enc;
        }
        // any other key is ignored

        drawScreen();
    }

    cout << "\nProgram terminated. Goodbye!\n";
    return 0;
}