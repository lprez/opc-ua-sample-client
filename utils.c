#include <stdio.h>
#include <string.h>
#include <open62541/types.h>
#include <open62541/client_config_default.h>

// Legge un file e copia il contenuto su una UA_ByteString.
UA_ByteString load_bytestring(char *path)
{
    UA_ByteString content = UA_STRING_NULL;
    FILE *fp;

    if (!(fp = fopen(path, "rb"))) {
        return content;
    }

    fseek(fp, 0, SEEK_END);
    content.length = ftell(fp);
    content.data = (UA_Byte *) UA_malloc(content.length * sizeof(UA_Byte));

    fseek(fp, 0, SEEK_SET);
    if (!fread(content.data, sizeof(UA_Byte), content.length, fp)) {
        UA_ByteString_clear(&content);
    }

    fclose(fp);
    return content;
}

// Legge una riga dal terminale.
int input_text(int buffer_size, char* buffer)
{
    int len;
    char c;

    if (fgets(buffer, buffer_size, stdin) != NULL) {
        len = strlen(buffer);

        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = 0;
            len -= 1;
        } else {
            while ((c = getc(stdin)) != '\n' && c != EOF);
        }

        return len;
    } else {
        return 0;
    }
}

// Legge un numero compreso tra 1 e n dal terminale
// e mostra le opzioni associate se items non è NULL.
int input_choice(int n, char** items)
{
    int i, choice = -1;
    char buffer[10];
    
    do {
        if (items != NULL) {
            for (i = 0; i < n; i++) {
                printf("%d) %s\n", i + 1, items[i]);
            }
        }
        
        input_text(sizeof(buffer), buffer);
    } while (sscanf(buffer, "%d", &choice) < 1 || choice <= 0 || choice > n);

    return choice - 1;
}

// Crea una bitmask elencando delle opzioni sul terminale
// associate ad ogni bit e chiedendo quali bit impostare.
unsigned int input_mask(int n, char** items)
{
    int i, item, abort = 0;
    unsigned int mask;
    char buffer[100], *token;

    if (items != NULL) {
        for (i = 0; i < n; i++) {
            if (items[i] != NULL) {
                printf("%d) %s\n", i, items[i]);
            }
        }
    }

    do {
        abort = 0;
        mask = 0;
        input_text(sizeof(buffer), buffer);
        token = strtok(buffer, ",");

        while (token != NULL) {
            if (sscanf(token, "%d", &item) < 1) {
                abort = 1;
            } else {
                if (item >= n) {
                    abort = 1;
                } else if (item > 0) {
                    mask |= 1 << (item - 1);
                }
            }
            token = strtok(NULL, ",");
        }

        if (abort) {
            puts("Formato non valido");
        }
    } while (abort);

    return mask;
}

// Domanda sì/no.
int input_sn(char *msg)
{
    char buffer[2];

    for(;;) {
        if (msg != NULL) {
            printf("%s [s/n] ", msg);
        } else {
            printf(" [s/n] ");
        }

        input_text(sizeof(buffer), buffer);

        if (buffer[0] == 's') {
            return 1;
        } else if (buffer[0] == 'n') {
            return 0;
        }
    }
}

// Crea un node ID leggendo dal terminale. Il secondo puntatore viene usato
// per rimandare la cancellazione di un eventuale stringa allocata nel caso
// in cui ciò vada fatto manualmente, altrimenti è NULL.
void input_node_id(UA_NodeId *nodeId, char **allocated_string)
{
    char buffer[100];
    UA_UInt32 node_id = 0;
    UA_UInt16 namespace_id = 0;

    do {
        printf("Inserisci il namespace: ");
        input_text(sizeof(buffer), buffer);
    } while (sscanf(buffer, "%d", &namespace_id) < 1);

    printf("Inserisci il node ID: ");
    input_text(sizeof(buffer), buffer);
    if (sscanf(buffer, "%d", &node_id) < 1) {
        if (allocated_string == NULL) {
            *nodeId = UA_NODEID_STRING_ALLOC(namespace_id, buffer);
        } else {
            *allocated_string = strdup(buffer);
            *nodeId = UA_NODEID_STRING(namespace_id, *allocated_string);
        }
    } else {
        *nodeId = UA_NODEID_NUMERIC(namespace_id, node_id);
    }
}

void print_variant(UA_Variant *value)
{
    int i, len;
    UA_DateTimeStruct datetime;

    if (UA_Variant_isScalar(value)) {
        len = 1;
    } else {
        len = value->arrayLength;
    }

    for (i = 0; i < len; i++) {
        if (value->type == &UA_TYPES[UA_TYPES_STRING]) {
            fwrite(((UA_String *) value->data)[i].data, ((UA_String *) value->data)[i].length, 1, stdout);
        } else if (value->type == &UA_TYPES[UA_TYPES_BOOLEAN]) {
            if (((UA_Boolean *) value->data)[i] == UA_TRUE) {
                printf("True");
            } else {
                printf("False");
            }
        } else if (value->type == &UA_TYPES[UA_TYPES_SBYTE]) {
            printf("%d", ((UA_SByte *) value->data)[i]);
        } else if (value->type == &UA_TYPES[UA_TYPES_BYTE]) {
            printf("%u", ((UA_Int32 *) value->data)[i]);
        } else if (value->type == &UA_TYPES[UA_TYPES_INT16]) {
            printf("%d", ((UA_Int16 *) value->data)[i]);
        } else if (value->type == &UA_TYPES[UA_TYPES_UINT16]) {
            printf("%u", ((UA_UInt16 *) value->data)[i]);
        } else if (value->type == &UA_TYPES[UA_TYPES_INT32]) {
            printf("%d", ((UA_Int32 *) value->data)[i]);
        } else if (value->type == &UA_TYPES[UA_TYPES_UINT32]) {
            printf("%u", ((UA_UInt32 *) value->data)[i]);
        } else if (value->type == &UA_TYPES[UA_TYPES_INT64]) {
            printf("%d", ((UA_Int64 *) value->data)[i]);
        } else if (value->type == &UA_TYPES[UA_TYPES_UINT64]) {
            printf("%u", ((UA_UInt64 *) value->data)[i]);
        } else if (value->type == &UA_TYPES[UA_TYPES_FLOAT]) {
            printf("%f", ((UA_Float *) value->data)[i]);
        } else if (value->type == &UA_TYPES[UA_TYPES_DOUBLE]) {
            printf("%f", ((UA_Double *) value->data)[i]);
        } else if (value->type == &UA_TYPES[UA_TYPES_DATETIME]) {
            datetime = UA_DateTime_toStruct(((UA_DateTime *) value->data)[i]);
            printf("%02d/%02d/%d %02d:%02d:%02d.%03d",
                    datetime.day, datetime.month, datetime.year,
                    datetime.hour, datetime.min, datetime.sec, datetime.milliSec);
        } else if (value->type == &UA_TYPES[UA_TYPES_NODEID]) {
            if (((UA_NodeId *) value->data)[i].identifierType == UA_NODEIDTYPE_NUMERIC) {
                printf("%d", ((UA_NodeId *) value->data)[i].identifier.numeric);
            } else {
                fwrite(((UA_NodeId *) value->data)[i].identifier.string.data,
                       ((UA_NodeId *) value->data)[i].identifier.string.length, 1, stdout);
            }
        } else {
            printf("(non in uno dei tipi supportati per la visualizzazione)");
        }

        if (i < len - 1) {
            printf(", ");
        } else {
            puts("");
        }
    }
}

void print_datavalue(UA_DataValue *value)
{
    UA_DateTimeStruct datetime;

    if (!value[0].hasValue) {
        puts("Valore letto = non valido");
    } else {
        printf("Valore letto = ");
        print_variant(&(value[0].value));
    }

    if (value[0].hasStatus) {
        #ifdef UA_ENABLE_STATUSCODE_DESCRIPTIONS
        printf("Status letto = %s\n", UA_StatusCode_name(value[0].status));
        #else
        printf("Status letto = %d\n", value[0].status);
        #endif
    }

    if (value[0].hasSourceTimestamp) {
        datetime = UA_DateTime_toStruct(value[0].sourceTimestamp);
        printf("Timestamp Source letto = %02d/%02d/%d %02d:%02d:%02d.%03d\n",
                datetime.day, datetime.month, datetime.year,
                datetime.hour, datetime.min, datetime.sec, datetime.milliSec);
    }
}
