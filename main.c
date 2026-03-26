#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

EM_ASYNC_JS(char*, get_inline_input, (const char* promptStr), {
    let prompt = UTF8ToString(promptStr);
    return await new Promise(resolve => {
        let output = document.getElementById('output-container');
        
        let promptSpan = document.createElement('span');
        promptSpan.innerText = '\n=> ' + prompt + ' ';
        output.appendChild(promptSpan);
        
        let inputField = document.createElement('input');
        inputField.type = 'text';
        inputField.className = 'inline-input';
        output.appendChild(inputField);
        inputField.focus();

        function handleEnter(e) {
            if (e.key === 'Enter') {
                let val = inputField.value;
                inputField.removeEventListener('keydown', handleEnter);
                
                inputField.remove();
                let textSpan = document.createElement('span');
                textSpan.className = 'user-inputted-text';
                textSpan.innerText = val + '\n';
                output.appendChild(textSpan);
                if (output.parentElement) output.parentElement.scrollTop = output.parentElement.scrollHeight;

                if(typeof FS !== 'undefined') FS.syncfs(false, function(err){});
                
                let lengthBytes = lengthBytesUTF8(val) + 1;
                let stringOnWasmHeap = _malloc(lengthBytes);
                stringToUTF8(val, stringOnWasmHeap, lengthBytes);
                resolve(stringOnWasmHeap);
            }
        }
        inputField.addEventListener('keydown', handleEnter);
    });
});
#endif

#define NUM_SHOWS 3
#define ROWS 5
#define COLS 10
#define SHOW_FILE "shows_data.dat"
#define BOOKING_FILE "bookings_data.dat"

// --- Struct Definitions ---
typedef struct {
    int showId;
    char title[50];
    int seats[ROWS][COLS]; // 0 = available, 1 = booked
    float price;
} Show;

typedef struct {
    int bookingId;
    char customerName[50];
    int showId;
    int numSeats;
    int bookedRows[50];
    int bookedCols[50];
    float totalAmount;
    int isActive; // 1 = Active, 0 = Cancelled
} Booking;

// Global array to hold show data in memory
Show shows[NUM_SHOWS];

// --- Function Prototypes ---
void initializeSystem();
void saveShowsData();
void clearInputBuffer();
int getValidInt(const char* prompt, int min, int max);
void printSeatMap(int showIdx);

void displayMenu();
void displayShows();
void bookTickets();
void viewBooking();
void cancelBooking();
void showOccupancyReport();

// --- Main Program ---
int main() {
    int choice;
    srand(time(NULL)); 
    initializeSystem(); // Load data from file or initialize hardcoded shows

    while (1) {
        displayMenu();
        choice = getValidInt("Enter your choice: ", 1, 6);

        switch (choice) {
            case 1: displayShows(); break;
            case 2: bookTickets(); break;
            case 3: viewBooking(); break;
            case 4: cancelBooking(); break;
            case 5: showOccupancyReport(); break;
            case 6:
                printf("\nExiting the system. Thank you!\n");
                exit(0);
        }
    }
    return 0;
}

// --- Helper Functions ---

void clearInputBuffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

// Reusable function to guarantee valid integer input within a range
int getValidInt(const char* prompt, int min, int max) {
    int value;
    while (1) {
#ifdef __EMSCRIPTEN__
        char* inputStr = get_inline_input(prompt);
        int rc = sscanf(inputStr, "%d", &value);
        free(inputStr);
        if (rc != 1) {
            clearInputBuffer();
#else
        printf("\n=> %s ", prompt);
        if (scanf("%d", &value) != 1) {
            clearInputBuffer();
#endif
            printf("[!] Invalid input. Please enter a number.\n");
        } else if (value < min || value > max) {
            printf("[!] Input out of range (%d - %d). Try again.\n", min, max);
        } else {
            return value;
        }
    }
}

void printSeatMap(int showIdx) {
    printf("\n--- Seat Layout for %s ---\n", shows[showIdx].title);
    printf("    ");
    for(int j=1; j<=COLS; j++) printf("S%02d ", j);
    printf("\n");
    for (int i = 0; i < ROWS; i++) {
        printf("R%02d ", i + 1);
        for (int j = 0; j < COLS; j++) {
            if (shows[showIdx].seats[i][j] == 0) printf("[_] "); // Available
            else printf("[X] "); // Booked
        }
        printf("\n");
    }
}

// --- Core Logic Functions ---

void initializeSystem() {
    FILE *file = fopen(SHOW_FILE, "rb");
    if (file != NULL) {
        fread(shows, sizeof(Show), NUM_SHOWS, file);
        fclose(file);
    } else {
        // Hardcode 3 shows if no save file exists
        for (int i = 0; i < NUM_SHOWS; i++) {
            shows[i].showId = i + 1;
            for (int r = 0; r < ROWS; r++) {
                for (int c = 0; c < COLS; c++) shows[i].seats[r][c] = 0; 
            }
        }
        strcpy(shows[0].title, "Inception");       shows[0].price = 150.0;
        strcpy(shows[1].title, "Interstellar");    shows[1].price = 180.0;
        strcpy(shows[2].title, "The Dark Knight"); shows[2].price = 200.0;
        saveShowsData();
    }
}

void saveShowsData() {
    FILE *file = fopen(SHOW_FILE, "wb");
    if (file != NULL) {
        fwrite(shows, sizeof(Show), NUM_SHOWS, file);
        fclose(file);
    }
}

void displayMenu() {
    printf("\n=========================================\n");
    printf("   MOVIE TICKET BOOKING SYSTEM\n");
    printf("=========================================\n");
    printf("1. Display Available Movies\n");
    printf("2. Book Tickets\n");
    printf("3. View Booking by ID\n");
    printf("4. Cancel a Booking\n");
    printf("5. Occupancy Report\n");
    printf("6. Exit\n");
}

void displayShows() {
    printf("\n--- Available Movies ---\n");
    for (int i = 0; i < NUM_SHOWS; i++) {
        printf("ID: %d | Movie: %-16s | Price: Rs. %.2f\n", shows[i].showId, shows[i].title, shows[i].price);
    }
}

void bookTickets() {
    Booking newB;
    displayShows();
    
    newB.showId = getValidInt("\nEnter Show ID to book: ", 1, NUM_SHOWS);
    int sIdx = newB.showId - 1;
    
    clearInputBuffer(); 
#ifdef __EMSCRIPTEN__
    char* nameStr = get_inline_input("Enter Customer Name:");
    strncpy(newB.customerName, nameStr, 49);
    newB.customerName[49] = '\0';
    free(nameStr);
#else
    printf("\n=> Enter Customer Name: ");
    fgets(newB.customerName, 50, stdin);
    newB.customerName[strcspn(newB.customerName, "\n")] = 0; 
#endif
    
    newB.numSeats = getValidInt("Enter number of seats to book (1-10): ", 1, 10);
    
    printSeatMap(sIdx);

    for (int k = 0; k < newB.numSeats; k++) {
        while (1) {
            printf("\nTicket %d:\n", k + 1);
            int r = getValidInt("Select Row (1-5):", 1, ROWS);
            int c = getValidInt("Select Seat (1-10):", 1, COLS);
            
            if (shows[sIdx].seats[r - 1][c - 1] == 1) {
                printf("[!] Seat R%dS%d is already booked! Choose another.\n", r, c);
            } else {
                shows[sIdx].seats[r - 1][c - 1] = 1; // Mark as booked
                newB.bookedRows[k] = r;
                newB.bookedCols[k] = c;
                break;
            }
        }
    }
    
    // Generate receipt and save
    newB.bookingId = rand() % 9000 + 1000;
    newB.totalAmount = newB.numSeats * shows[sIdx].price;
    newB.isActive = 1;
    
    FILE *bFile = fopen(BOOKING_FILE, "ab");
    if (bFile != NULL) {
        fwrite(&newB, sizeof(Booking), 1, bFile);
        fclose(bFile);
    }
    saveShowsData();
    
    printf("\n=== BOOKING SUCCESSFUL ===\n");
    printf("Booking ID    : %d\n", newB.bookingId);
    printf("Customer      : %s\n", newB.customerName);
    printf("Movie         : %s\n", shows[sIdx].title);
    printf("Seats Booked  : %d\n", newB.numSeats);
    printf("Seat Numbers  : ");
    for (int i = 0; i < newB.numSeats; i++) {
        printf("R%dS%d ", newB.bookedRows[i], newB.bookedCols[i]);
    }
    printf("\nTotal Amount  : Rs. %.2f\n", newB.totalAmount);
    printf("===============================\n");
}

void viewBooking() {
    Booking b;
    int searchId = getValidInt("\nEnter Booking ID to search: ", 1000, 9999);
    int found = 0;

    FILE *bFile = fopen(BOOKING_FILE, "rb");
    if (bFile == NULL) { printf("[!] No bookings found.\n"); return; }

    while (fread(&b, sizeof(Booking), 1, bFile)) {
        if (b.bookingId == searchId) {
            printf("\n--- Booking Details ---\n");
            printf("Status        : %s\n", b.isActive ? "ACTIVE" : "CANCELLED");
            printf("Customer Name : %s\n", b.customerName);
            printf("Movie ID      : %d\n", b.showId);
            printf("Seats Booked  : ");
            for(int i=0; i<b.numSeats; i++) printf("R%dS%d ", b.bookedRows[i], b.bookedCols[i]);
            printf("\nTotal Amount  : Rs. %.2f\n", b.totalAmount);
            printf("-----------------------\n");
            found = 1;
            break;
        }
    }
    fclose(bFile);
    if (!found) printf("[!] Booking ID %d not found.\n", searchId);
}

void cancelBooking() {
    Booking b;
    int searchId = getValidInt("\nEnter Booking ID to cancel: ", 1000, 9999);
    int found = 0;

    FILE *bFile = fopen(BOOKING_FILE, "r+b");
    if (bFile == NULL) { printf("[!] No bookings found.\n"); return; }

    while (fread(&b, sizeof(Booking), 1, bFile)) {
        if (b.bookingId == searchId) {
            found = 1;
            if (b.isActive == 0) {
                printf("[!] This booking is already cancelled.\n");
                break;
            }
            
            // Mark as cancelled
            b.isActive = 0;
            
            // Move file pointer back to overwrite this specific record
            fseek(bFile, -(long)sizeof(Booking), SEEK_CUR);
            fwrite(&b, sizeof(Booking), 1, bFile);
            
            // Free the seats in the shows array
            int sIdx = b.showId - 1;
            for(int i = 0; i < b.numSeats; i++) {
                shows[sIdx].seats[b.bookedRows[i] - 1][b.bookedCols[i] - 1] = 0;
            }
            saveShowsData();
            
            printf("\n[!] Booking %d successfully cancelled. Seats have been freed.\n", searchId);
            break;
        }
    }
    fclose(bFile);
    if (!found) printf("[!] Booking ID %d not found.\n", searchId);
}

void showOccupancyReport() {
    printf("\n=== OCCUPANCY REPORT ===\n");
    int totalSeats = ROWS * COLS;
    
    for (int i = 0; i < NUM_SHOWS; i++) {
        int bookedCount = 0;
        for (int r = 0; r < ROWS; r++) {
            for (int c = 0; c < COLS; c++) {
                if (shows[i].seats[r][c] == 1) bookedCount++;
            }
        }
        
        float occupancyPercent = ((float)bookedCount / totalSeats) * 100.0;
        
        printf("\nMovie: %-15s | Occupancy: %05.2f%%\n", shows[i].title, occupancyPercent);
        printf("  Seats: %d Booked / %d Available / %d Total\n", bookedCount, totalSeats - bookedCount, totalSeats);
    }
    printf("\n========================\n");
}