#include <stdio.h>
#include <string.h>

void test(char* name) {
    // test in functions
    //
    printf("\nfunction test: %s\n",name);
}

int main() {
    // 1. Basic string declaration and output
    char greeting[] = "Hello, world!";
    printf("%s\n", greeting);

    // 2. String concatenation (using strcat)
    char firstName[] = "John";
    char lastName[] = "Doe";
    char fullName[50]; 

    strcpy(fullName, firstName); 
    strcat(fullName, " ");
    strcat(fullName, lastName);

    // 2.1
    test(fullName);

    // 2.2
    printf("\nName Test %s\n", fullName);


    // 3. Using sprintf for formatting
    int age = 30;
    char ageString[20]; 

    sprintf(ageString, "Age: %d", age);
    printf("\n%s\n", ageString);


    // 4. Iterating over characters in a string
    printf("\n");
    char word[] = "Example";
    for (int i = 0; i < strlen(word); i++) {
        printf("%c ", word[i]);
    }
    printf("\n");


    // 5. String manipulation (finding substrings using strstr)
    char sentence[] = "This is a sample sentence.";
    char* searchWord = "sample";
    char* found = strstr(sentence, searchWord);

    if (found != NULL) {
        printf("\nFound '%s' at position %ld\n", searchWord, found - sentence);
    } else {
        printf("\nSubstring not found.\n");
    }


    // 6. dupe of 1.
    printf("\n%s\n", greeting);

    return 0;
}
