/**
 * @file main.c
 * @brief Warehouse Management System
 *
 * This program implements a Warehouse Management System that allows users to manage categories and products in a warehouse.
 * It provides functionalities such as adding categories, adding products to categories, updating product stock, decreasing product stock,
 * displaying all categories and products, and user authentication.
 *
 * The program uses a binary search tree to store categories and a linked list to store products within each category.
 * It also utilizes the libcurl library to make HTTP requests to retrieve temperature data from OpenWeatherMap API.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include <pthread.h>

/**
 * @struct Product
 * @brief Represents a product.
 * 
 * This struct contains information about a product, including its ID, name, quantity, and a pointer to the next product.
 */
typedef struct Product {
    int productId;              /**< The ID of the product. */
    char name[50];              /**< The name of the product. */
    int quantity;               /**< The quantity of the product. */
    char category[50];          /**< The category of the product. */
    struct Product *next;       /**< Pointer to the next product in the list. */
} Product;

/**
 * @struct CategoryNode
 * @brief Represents a category node.
 * 
 * This struct represents a node in the category tree. It contains information about the category, a pointer to the head of the products list, and pointers to the left and right child nodes.
 */
typedef struct CategoryNode {
    char category[50];          /**< The name of the category. */
    Product *productsHead;      /**< Pointer to the head of the products list. */
    struct CategoryNode *left;  /**< Pointer to the left child node. */
    struct CategoryNode *right; /**< Pointer to the right child node. */
} CategoryNode;

/**
 * @struct AnalysisResult
 * @brief Represents the result of product analysis.
 * 
 * This struct contains the result of the product analysis, including low stock products, high stock products, max stock product, min stock product, total quantity, and average quantity.
 */
typedef struct AnalysisResult {
    Product *lowStockProducts;
    Product *highStockProducts;
    Product *maxStockProduct;
    Product *minStockProduct;
    int totalQuantity;
    double averageQuantity;
} AnalysisResult;

CategoryNode *root = NULL;      /**< Pointer to the root of the category tree. */

// Function prototypes and declarations
CategoryNode* insertCategory(CategoryNode* node, char* category);
Product* insertProduct(Product* head, int productId, char* name, int quantity);
void updateProduct(CategoryNode* node, int productId, int newQuantity);
Product* findProduct(Product* head, int productId);
void deleteProduct(CategoryNode* node, int productId, int decreaseQuantity);
void displayCategories(CategoryNode* node);
void displayProducts(Product* head);
CategoryNode* findCategory(CategoryNode* node, char* category);
int login(const char *username, const char *password);
int checkCredentialsFromFile(const char *username, const char *password);
void menu();
double getTemperature(const char *apiKey);


/**
 * @brief Callback function to write data received from HTTP request.
 * 
 * This function is used as a callback by libcurl to write data received from an HTTP request to a buffer.
 * 
 * @param data Pointer to the data received.
 * @param size Size of each data element.
 * @param nmemb Number of data elements.
 * @param userp Pointer to the user data buffer.
 * @return Size of the data written.
 */
static size_t write_callback(void *data, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    strcat((char *)userp, (char *)data);
    return realsize;
}

/**
 * @brief Get the temperature at a specific location using OpenWeatherMap API.
 * 
 * This function makes an HTTP request to the OpenWeatherMap API to retrieve the current temperature at a specific location.
 * The temperature is returned in Fahrenheit.
 * 
 * @param apiKey The API key for accessing the OpenWeatherMap API.
 * @return The temperature in Fahrenheit, or -1 if an error occurs.
 */
double getTemperature(const char *apiKey) {
    CURL *curl;
    CURLcode res;
    double temperature = 0.0;
    char url[256] = {0};
    sprintf(url, "https://api.openweathermap.org/data/2.5/weather?q=West%%20Lafayette&appid=%s&units=imperial", apiKey);
    if (res != CURLE_OK) {
        fprintf(stderr, "\n", curl_easy_strerror(res));
        return -1;  
    }
;
    // Set HTTP headers
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Accept: application/json");
    char response[1024] = {0};

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
        res = curl_easy_perform(curl);
        if (res != CURLE_OK)
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));

        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);

        // Parse JSON
        struct json_object *parsed_json;
        struct json_object *main;
        struct json_object *temp;

        parsed_json = json_tokener_parse(response);
        json_object_object_get_ex(parsed_json, "main", &main);
        json_object_object_get_ex(main, "temp", &temp);
        temperature = json_object_get_double(temp);
        json_object_put(parsed_json); 
    }
    return temperature;
}

/**
 * @brief Insert a new category into the category tree.
 * 
 * This function inserts a new category into the category tree.
 * 
 * @param node Pointer to the current node in the category tree.
 * @param category The name of the category to insert.
 * @return Pointer to the updated category tree.
 */
CategoryNode* insertCategory(CategoryNode* node, char* category) {
    if (node == NULL) {
        CategoryNode *newNode = (CategoryNode*) malloc(sizeof(CategoryNode));
        strcpy(newNode->category, category);
        newNode->productsHead = NULL;
        newNode->left = newNode->right = NULL;
        return newNode;
    }
    if (strcmp(category, node->category) < 0)
        node->left = insertCategory(node->left, category);
    else if (strcmp(category, node->category) > 0)
        node->right = insertCategory(node->right, category);
    return node;
}

/**
 * @brief Delete a category from the category tree.
 * 
 * This function deletes a category from the category tree.
 * 
 * @param node Pointer to the current node in the category tree.
 * @param category The name of the category to delete.
 * @return Pointer to the updated category tree.
 */
CategoryNode* deleteCategory(CategoryNode* node, char* category) {
    if (node == NULL) return NULL;
    if (strcmp(category, node->category) < 0)
        node->left = deleteCategory(node->left, category);
    else if (strcmp(category, node->category) > 0)
        node->right = deleteCategory(node->right, category);
    else {
        if (node->left == NULL) {
            CategoryNode *temp = node->right;
            free(node);
            return temp;
        } else if (node->right == NULL) {
            CategoryNode *temp = node->left;
            free(node);
            return temp;
        }
        CategoryNode *temp = node->right;
        while (temp->left != NULL) {
            temp = temp->left;
        }
        strcpy(node->category, temp->category);
        node->right = deleteCategory(node->right, temp->category);
    }
    return node;
}

/**
 * @brief Insert a new product into the products list.
 * 
 * This function inserts a new product into the products list of a category.
 * 
 * @param head Pointer to the head of the products list.
 * @param productId The ID of the product to insert.
 * @param name The name of the product.
 * @param quantity The quantity of the product.
 * @return Pointer to the updated products list.
 */
Product* insertProduct(Product* head, int productId, char* name, int quantity) {
    Product *newProduct = (Product*) malloc(sizeof(Product));
    newProduct->productId = productId;
    strcpy(newProduct->name, name);
    newProduct->quantity = quantity;
    strcpy(newProduct->category, name);
    newProduct->next = head;
    return newProduct;
}

/**
 * @brief Write product information to a file.
 * 
 * This function writes the information of a product to a file.
 * 
 * @param product Pointer to the product to write.
 * @param filename The name of the file to write to.
 */
void writeProductToFile(Product* product, char* filename) {
    FILE* file = fopen(filename, "a");
    if (file == NULL) {
        printf("Unable to open file\n");
        exit(1);
    }

    fprintf(file, "%d,%s,%d,%s\n", product->productId, product->name, product->quantity, product->category);
    fclose(file);
}

/**
 * @brief Read products from a file.
 * 
 * This function reads product information from a file and returns a linked list of products.
 * 
 * @param filename The name of the file to read from.
 * @return Pointer to the head of the products list.
 */
Product* readProductsFromFile(char* filename) {
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        printf("Unable to open file\n");
        return NULL;
    }

    Product* head = NULL;
    int productId;
    char name[50];
    int quantity;
    char category[50];

    while (fscanf(file, "%d,%49[^,],%d,%49[^,\n]", &productId, name, &quantity, category) == 4) {
        head = insertProduct(head, productId, name, quantity, category);
    }

    fclose(file);
    return head;
}


/**
 * @brief Update the quantity of a product in the products list.
 * 
 * This function updates the quantity of a product in the products list based on the product ID.
 * 
 * @param node Pointer to the current node in the category tree.
 * @param productId The ID of the product to update.
 * @param newQuantity The new quantity of the product.
 */
void updateProduct(CategoryNode* node, int productId, int newQuantity) {
    if (node == NULL) return;
    updateProduct(node->left, productId, newQuantity);
    Product* product = findProduct(node->productsHead, productId);
    if (product != NULL) {
        product->quantity = newQuantity;
        printf("Product quantity updated to %d.\n", newQuantity);
        return;
    }
    updateProduct(node->right, productId, newQuantity);
}

/**
 * @brief Find a product by ID in the products list.
 * 
 * This function finds a product with a specific ID in the products list.
 * 
 * @param head Pointer to the head of the products list.
 * @param productId The ID of the product to find.
 * @return Pointer to the product if found, NULL otherwise.
 */
Product* findProduct(Product* head, int productId) {
    while (head != NULL) {
        if (head->productId == productId) {
            return head;
        }
        head = head->next;
    }
    return NULL;
}

/**
 * @brief Delete a product from the products list.
 * 
 * This function deletes a product from the products list based on the product ID and decreases the quantity by a specified amount.
 * 
 * @param node Pointer to the current node in the category tree.
 * @param productId The ID of the product to delete.
 * @param decreaseQuantity The quantity to decrease.
 */
void deleteProduct(CategoryNode* node, int productId, int decreaseQuantity) {
    if (node == NULL) return;
    deleteProduct(node->left, productId, decreaseQuantity);
    Product* product = findProduct(node->productsHead, productId);
    if (product != NULL) {
        if (product->quantity >= decreaseQuantity) {
            product->quantity -= decreaseQuantity;
            printf("Decreased quantity by %d. New quantity: %d\n", decreaseQuantity, product->quantity);
        } else {
            printf("Not enough stock to decrease by %d. Current stock: %d\n", decreaseQuantity, product->quantity);
        }
        return;
    }
    deleteProduct(node->right, productId, decreaseQuantity);
}

/**
 * @brief Display all categories and products in the category tree.
 * 
 * This function displays all categories and products in the category tree.
 * 
 * @param node Pointer to the current node in the category tree.
 */
void displayCategories(CategoryNode* node) {
    if (node != NULL) {
        displayCategories(node->left);
        printf("Category: %s\n", node->category);
        displayProducts(node->productsHead);
        displayCategories(node->right);
    }
}

/**
 * @brief Display all products in the products list.
 * 
 * This function displays all products in the products list.
 * 
 * @param head Pointer to the head of the products list.
 */
void displayProducts(Product* head) {
    Product *temp = head;
    while (temp != NULL) {
        printf("  Product ID: %d, Name: %s, Quantity: %d\n", temp->productId, temp->name, temp->quantity);
        temp = temp->next;
    }
}

/**
 * @brief Print the products report.
 * 
 * This function prints a report of all products in the products list.
 * 
 * @param head Pointer to the head of the products list.
 */
void printProductsReport(Product* head) {
    Product* current = head;
    Product* index = NULL;
    Product* temp;

    // Sorting the list of products based on their ID
    for(current = head; current != NULL; current = current->next) {
        for(index = current->next; index != NULL; index = index->next) {
            if(current->productId > index->productId) {
                temp = current;
                current = index;
                index = temp;
            }
        }
    }

    // Printing the sorted list of products
    printf("Product ID, Product Name, Product Quantity, Product Category\n");
    for(current = head; current != NULL; current = current->next) {
        printf("%d, %s, %d, %s\n", current->productId, current->name, current->quantity, current->category);
    }
}

/**
 * @brief Find a category by name in the category tree.
 * 
 * This function finds a category with a specific name in the category tree.
 * 
 * @param node Pointer to the current node in the category tree.
 * @param category The name of the category to find.
 * @return Pointer to the category if found, NULL otherwise.
 */
CategoryNode* findCategory(CategoryNode* node, char* category) {
    if (node == NULL) return NULL;
    if (strcmp(category, node->category) == 0) return node;
    if (strcmp(category, node->category) < 0)
        return findCategory(node->left, category);
    else
        return findCategory(node->right, category);
}

/**
 * @struct AnalysisResult
 * @brief Represents the result of product analysis.
 * 
 * This struct contains the result of the product analysis, including low stock products, high stock products, max stock product, min stock product, total quantity, and average quantity.
 */
AnalysisResult analyzeProducts(Product *head) {
    int count = 0;
    int totalQuantity = 0;
    Product *current = head;
    Product *lowStockProducts = NULL;
    Product *highStockProducts = NULL;
    Product *maxStockProduct = NULL;
    Product *minStockProduct = NULL;

    while (current != NULL) {
        totalQuantity += current->quantity;
        count++;
        if (maxStockProduct == NULL || current->quantity > maxStockProduct->quantity) {
            maxStockProduct = current;
        }
        if (minStockProduct == NULL || current->quantity < minStockProduct->quantity) {
            minStockProduct = current;
        }
        current = current->next;
    }

    double averageQuantity = (double)totalQuantity / count;

    current = head;
    while (current != NULL) {
        if (current->quantity < averageQuantity) {
            // Add to low stock products list
            Product *newProduct = malloc(sizeof(Product));
            *newProduct = *current;
            newProduct->next = lowStockProducts;
            lowStockProducts = newProduct;
        } else if (current->quantity > averageQuantity) {
            // Add to high stock products list
            Product *newProduct = malloc(sizeof(Product));
            *newProduct = *current;
            newProduct->next = highStockProducts;
            highStockProducts = newProduct;
        }
        current = current->next;
    }

    AnalysisResult result = {lowStockProducts, highStockProducts, maxStockProduct, minStockProduct, totalQuantity, averageQuantity};
    return result;
}

/**
 * @brief Print the analysis report of products.
 * 
 * This function prints the analysis report of products, including total quantity, average quantity, max stock product, min stock product, low stock products, and high stock products.
 * 
 * @param head Pointer to the head of the products list.
 */
void printAnalysisReport(Product *head) {
    AnalysisResult result = analyzeProducts(head);
    printf("Total quantity: %d\n", result.totalQuantity);
    printf("Average quantity: %.2f\n", result.averageQuantity);
    printf("Max stock product ID: %d, Quantity: %d\n", result.maxStockProduct->productId, result.maxStockProduct->quantity);
    printf("Min stock product ID: %d, Quantity: %d\n", result.minStockProduct->productId, result.minStockProduct->quantity);
    printf("Low stock products:\n");
    Product *current = result.lowStockProducts;
    while (current != NULL) {
        printf("Product ID: %d, Quantity: %d\n", current->productId, current->quantity);
        current = current->next;
    }
    printf("High stock products:\n");
    current = result.highStockProducts;
    while (current != NULL) {
        printf("Product ID: %d, Quantity: %d\n", current->productId, current->quantity);
        current = current->next;
    }
}

/**
 * @brief Check user credentials for login.
 * 
 * This function checks the user credentials for login. It first checks the hardcoded superadmin credentials,
 * and then checks the credentials from a file.
 * 
 * @param username The username to check.
 * @param password The password to check.
 * @return 1 if login is successful, 0 otherwise.
 */
int login(const char *username, const char *password) {
    const char *correctUsername = "superadmin";
    const char *correctPassword = "admin123";

    if (strcmp(username, correctUsername) == 0 && strcmp(password, correctPassword) == 0) {
        return 1;
    }

    return checkCredentialsFromFile(username, password);
}

/**
 * @brief Check user credentials from a file.
 * 
 * This function checks the user credentials from a file. The file "user.txt" contains a list of usernames and passwords.
 * 
 * @param username The username to check.
 * @param password The password to check.
 * @return 1 if the credentials are valid, 0 otherwise.
 */
int checkCredentialsFromFile(const char *username, const char *password) {
    FILE *file = fopen("user.txt", "r");
    if (!file) {
        perror("Failed to open user.txt");
        return 0;
    }

    char fileUsername[50], filePassword[50];
    while (fscanf(file, "%49[^,], %49s", fileUsername, filePassword) == 2) {
        if (strcmp(username, fileUsername) == 0 && strcmp(password, filePassword) == 0) {
            fclose(file);
            return 1;
        }
    }

    fclose(file);
    return 0;
}

/**
 * @brief Display the menu for the Warehouse Management System.
 * 
 * This function displays the menu for the Warehouse Management System and handles user input to perform various operations.
 */
void menu() {
    const char *apiKey = "15171717a4ebf7b0e5f4899ff2455fa1"; 

    int choice, productId, quantity;
    char input[100], category[50], productName[50], username[50], password[50];

    // Login
    printf("Enter username: ");
    fgets(username, sizeof(username), stdin);
    username[strcspn(username, "\n")] = 0;

    printf("Enter password: ");
    fgets(password, sizeof(password), stdin);
    password[strcspn(password, "\n")] = 0;

    if (!login(username, password)) {
        printf("Login failed. Invalid username or password.\n");
        return;
    }

    // Login successful
    printf("Login successful!\n");
     double temperature = getTemperature(apiKey);

    // Display temperature
    if (temperature != -1) {  
        printf("Warehouse location temperature: %.2f°F\n", temperature);
        if (temperature > 100) {
            printf("It's hot! Turning on the air conditioning.\n");
        } else if (temperature < 40) {
            printf("It's cold! Turning on the heating.\n");
        } else {
            printf("Temperature is comfortable. No need for heating or air conditioning.\n");
        }
    } else {
        printf("\n");
    }
    fflush(stdout);  
    do {
        // Display menu
        double temperature = getTemperature(apiKey);
        if (temperature == 0) {
            printf("\n");
        } else {
            printf("Warehouse location temperature: %.2f°F\n", temperature);
            if (temperature > 100) {
                printf("Alert: Excessive heat detected! Activating air conditioning to maintain optimal product storage conditions.\n");
            } else if (temperature < 40) {
                printf("Alert: Cold temperatures detected! Activating heating to prevent product damage from freezing.\n");
            } else {
                printf("Warehouse temperature is within the optimal range. No climate control adjustments needed.\n");
            }
        }
        fflush(stdout); 

        printf("\nWarehouse Management System Menu:\n");
        printf("1. Add Category\n");
        printf("2. Delete Category\n");
        printf("3. Add Product to a Category\n");
        printf("4. Update Product Stock\n");
        printf("5. Decrease Product Stock\n");
        printf("6. Display All Categories and Products\n");
        printf("7. Analyze Products\n");
        printf("8. Print Products list\n");
        printf("9. Exit\n");
        printf("Enter your choice: ");
        fgets(input, sizeof(input), stdin);
        sscanf(input, "%d", &choice);

        switch (choice) {
        
            case 1:
                // Add Category
                printf("Enter category name: ");
                fgets(category, sizeof(category), stdin);
                category[strcspn(category, "\n")] = 0;
                root = insertCategory(root, category);
                printf("Category '%s' added successfully.\n", category);
                break;
            case 2:
                // Delete Category
                printf("Enter category name to delete: ");
                fgets(category, sizeof(category), stdin);
                category[strcspn(category, "\n")] = 0;
                root = deleteCategory(root, category);
                printf("Category '%s' deleted successfully.\n", category);
                break;
            case 3:
                // Add Product to Category
                printf("Existing Categories:\n");
                displayCategories(root);
                printf("Enter category name where to add product: ");
                fgets(category, sizeof(category), stdin);
                category[strcspn(category, "\n")] = 0;

                printf("Enter product ID: ");
                fgets(input, sizeof(input), stdin);
                sscanf(input, "%d", &productId);

                printf("Enter product name: ");
                fgets(productName, sizeof(productName), stdin);
                productName[strcspn(productName, "\n")] = 0;

                printf("Enter quantity: ");
                fgets(input, sizeof(input), stdin);
                sscanf(input, "%d", &quantity);

                if (findCategory(root, category)) {
                    findCategory(root, category)->productsHead = insertProduct(findCategory(root, category)->productsHead, productId, productName, quantity);
                    printf("Product added successfully.\n");
                } else {
                    printf("Category does not exist. Please create the category first.\n");
                }
                break;
            case 4:
                // Update Product Stock
                printf("Enter product ID to increase stock: ");
                fgets(input, sizeof(input), stdin);
                sscanf(input, "%d", &productId);
                printf("Enter new quantity: ");
                fgets(input, sizeof(input), stdin);
                sscanf(input, "%d", &quantity);
                updateProduct(root, productId, quantity);
                break;
            case 5:
                // Decrease Product Stock
                printf("Enter product ID to decrease stock: ");
                fgets(input, sizeof(input), stdin);
                sscanf(input, "%d", &productId);
                printf("Enter quantity to decrease: ");
                fgets(input, sizeof(input), stdin);
                sscanf(input, "%d", &quantity);
                deleteProduct(root, productId, quantity);
                break;
            case 6:
                // Display All Categories and Products
                printf("All Categories and Products:\n");
                displayCategories(root);
                break;
            case 7:
                // Analyze Products
                printf("Analysis Report:\n");
                printAnalysisReport(root->productsHead);
                break;
            case 8:
                // Print Inventory list
                printf("Inventory List:\n");
                printProductsReport(root->productsHead);
                break;
            case 9:
                // Exit
                printf("Exiting...\n");
                break;
            default:
                printf("Invalid choice. Please try again.\n");
                break;
        }
    } while (choice != 6);
}


int main() {
    Product* head = readProductsFromFile("products.txt");
    menu();
    return 0;
}
