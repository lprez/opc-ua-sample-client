#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <open62541/types.h>
#include <open62541/client_config_default.h>
#include <open62541/client_highlevel.h>
#include <open62541/client_subscriptions.h>

#ifdef PTHREAD
#include <pthread.h>
typedef struct {
    UA_Client *client;
    pthread_mutex_t lock;
    int close;
} client_data_t;
#else
typedef struct {
    UA_Client *client;
} client_data_t;
#endif

extern UA_ByteString load_bytestring(char *path);
extern int input_text(int buffer_size, char* buffer);
extern int input_choice(int n, char** items);
extern unsigned int input_mask(int n, char** items);
extern int input_sn(char *msg);
extern void input_node_id(UA_NodeId *nodeId, char **allocated_string);
extern void print_variant(UA_Variant *value);
extern void print_datavalue(UA_DataValue *value);

UA_Client *client_connect(char *uri)
{
    UA_Client *client;
    UA_ClientConfig *config;
    UA_EndpointDescription *endpoints = NULL;
    UA_ByteString certificate, private_key;

    char uri_buffer[100], certpath[100], keypath[100], username[100], password[100],
         *msg_choose_endpoint[] = {
            "Discovery degli endpoint",
            "Connessione a un endpoint (scelto in base alla configurazione default)"
        };
    size_t endpoints_size = 0;
    int i, choice, err_code;

    // Configurazione client
    client = UA_Client_new();
    config = UA_Client_getConfig(client);

    printf("File certificato (filename in formato .der, oppure lasciare vuoto): ");
    if (input_text(sizeof(certpath), certpath) > 0) {
        #ifdef UA_ENABLE_ENCRYPTION
        certificate = load_bytestring(certpath);

        printf("File chiave privata (formato .der): ");
        input_text(sizeof(keypath), keypath);
        private_key = load_bytestring(keypath);

        UA_ClientConfig_setDefaultEncryption(config, certificate, private_key, NULL, 0, NULL, 0);

        UA_ByteString_deleteMembers(&certificate);
        UA_ByteString_deleteMembers(&private_key);

        printf("Application URI (dev'essere uguale all'URI del certificato) [DEFAULT: urn:unconfigured:application]: ");
        if (input_text(sizeof(uri_buffer), uri_buffer) > 0) {
            UA_clear(&config->clientDescription.applicationUri, &UA_TYPES[UA_TYPES_STRING]);
            config->clientDescription.applicationUri = UA_STRING_ALLOC(uri_buffer);
        }
        #else
        puts("Errore: Crittografia non abilitata, compilare open62541 e client-tutorial con UA_ENABLE_ENCRYPTION");
        UA_ClientConfig_setDefault(config);
        #endif
    } else {
        UA_ClientConfig_setDefault(config);
    }

    choice = input_choice(2, msg_choose_endpoint);

    if (uri == NULL) {
        printf("Inserisci l'URI: ");

        while (!input_text(sizeof(uri_buffer), uri_buffer));

        uri = uri_buffer;
    }

    if (choice == 0) {
        if (UA_Client_getEndpoints(client, uri, &endpoints_size, &endpoints) != UA_STATUSCODE_GOOD) {
            puts("Ricerca degli endpoint non riuscita");
            UA_Client_disconnect(client);
            UA_Client_delete(client);
            return NULL;
        }

        puts("Scegli un endpoint:");
        for (i = 0; i < endpoints_size; i++) {
            printf("Endpoint %d:", i + 1);
            printf("\n URI: ");
            fwrite(endpoints[i].endpointUrl.data, endpoints[i].endpointUrl.length, 1, stdout);
            printf("\n Security mode: ");
            switch (endpoints[i].securityMode) {
                case UA_MESSAGESECURITYMODE_NONE:
                    printf("None");
                    break;
                case UA_MESSAGESECURITYMODE_SIGN:
                    printf("Sign");
                    break;
                case UA_MESSAGESECURITYMODE_SIGNANDENCRYPT:
                    printf("SignAndEncrypt");
                    break;
                default:
                    printf("Non valido");
                    break;
            }
            printf("\n Security policy: ");
            fwrite(endpoints[i].securityPolicyUri.data,
                   endpoints[i].securityPolicyUri.length, 1, stdout);
            printf("\n Transport protocol: ");
            fwrite(endpoints[i].transportProfileUri.data,
                   endpoints[i].transportProfileUri.length, 1, stdout);
            printf("\n Security level: %d\n", endpoints[i].securityLevel);
            puts("");
        }
        printf("Inserisci il numero dell'endpoint a cui connettersi: ");
        choice = input_choice(endpoints_size, NULL);
        UA_copy(&(endpoints[choice]), &(config->endpoint), &UA_TYPES[UA_TYPES_ENDPOINTDESCRIPTION]);
        strncpy(uri_buffer, endpoints[choice].endpointUrl.data,
                endpoints[choice].endpointUrl.length);
        uri_buffer[endpoints[choice].endpointUrl.length] = '\0';
        uri = uri_buffer;
    }

    if (endpoints != NULL) {
        UA_Array_delete(endpoints, endpoints_size, &UA_TYPES[UA_TYPES_ENDPOINTDESCRIPTION]);
    }

    // Connessione all'endpoint
    printf("Username [lascia vuoto per anonimo]: ");
    if (input_text(sizeof(username), username) > 0) {
        printf("Password: ");
        input_text(sizeof(password), password);
        err_code = UA_Client_connect_username(client, uri, username, password);
    } else {
        err_code = UA_Client_connect(client, uri);
    }

    if (err_code != UA_STATUSCODE_GOOD) {
        UA_Client_delete(client);
        return NULL;
    }

    return client;
}

void browse(UA_Client *client)
{
    char buffer[100], *msg_select_root[] = {
            "RootFolder",
            "ObjectsFolder",
            "ViewsFolder",
            "TypesFolder",
            "Server",
            "NodeID a scelta"
        },
        *msg_nodeclass_mask[] = {
            NULL,
            "Object",
            "Variable",
            "Method",
            "ObjectType",
            "VariableType",
            "ReferenceType",
            "DataType",
            "View"
        };
    int i, j, string_id = 0;
    UA_BrowseRequest request;
    UA_BrowseResponse response;
    UA_ReferenceDescription *ref = NULL;

    UA_BrowseRequest_init(&request);
    request.nodesToBrowseSize = 1;
    request.nodesToBrowse = UA_BrowseDescription_new();

    switch (input_choice(6, msg_select_root)) {
        case 0:
            request.nodesToBrowse[0].nodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ROOTFOLDER);
            break;

        case 1:
            request.nodesToBrowse[0].nodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
            break;

        case 2:
            request.nodesToBrowse[0].nodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_VIEWSFOLDER);
            break;

        case 3:
            request.nodesToBrowse[0].nodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_TYPESFOLDER);
            break;

        case 4:
            request.nodesToBrowse[0].nodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_SERVER);
            break;

        default:
            input_node_id(&(request.nodesToBrowse[0].nodeId), NULL);
    }

    request.nodesToBrowse[0].resultMask = UA_BROWSERESULTMASK_ALL;
    request.nodesToBrowse[0].includeSubtypes = UA_TRUE;
    request.requestedMaxReferencesPerNode = 0;

    if (input_sn("Vuoi impostare filtro per la NodeClass?")) {
        puts("Inserisci le classi da visualizzare separate da una virgola (es. '1,2')");
        request.nodesToBrowse[0].nodeClassMask = input_mask(9, msg_nodeclass_mask);
    }

    response = UA_Client_Service_browse(client, request);

    for (i = 0; i < response.resultsSize; i++) {
        puts("");

        for (j = 0; j < response.results[i].referencesSize; j++) {
            ref = &(response.results[i].references[j]);

            printf("Nodo ");
            if (ref->nodeId.nodeId.identifierType == UA_NODEIDTYPE_NUMERIC) {
                printf("%d", ref->nodeId.nodeId.identifier.numeric);
            } else {
                fwrite(ref->nodeId.nodeId.identifier.string.data,
                       ref->nodeId.nodeId.identifier.string.length, 1, stdout);
            }
            printf(" nel namespace %d:", ref->nodeId.nodeId.namespaceIndex);

            printf("\n Namespace URI: ");
            fwrite(ref->nodeId.namespaceUri.data, ref->nodeId.namespaceUri.length, 1, stdout);
            printf("\n BrowseName: ");
            fwrite(ref->browseName.name.data, ref->browseName.name.length, 1, stdout);
            printf("\n DisplayName: ");
            fwrite(ref->displayName.text.data, ref->displayName.text.length, 1, stdout);

            printf("\n NodeClass: ");
            switch (ref->nodeClass) {
                case UA_NODECLASS_UNSPECIFIED:
                    puts("Non specificato");
                    break;

                case UA_NODECLASS_OBJECT:
                    puts("Object");
                    break;
                case UA_NODECLASS_VARIABLE:
                    puts("Variable");
                    break;
                case UA_NODECLASS_METHOD:
                    puts("Method");
                    break;
                case UA_NODECLASS_OBJECTTYPE:
                    puts("ObjectType");
                    break;
                case UA_NODECLASS_VARIABLETYPE:
                    puts("VariableType");
                    break;
                case UA_NODECLASS_REFERENCETYPE:
                    puts("ReferenceType");
                    break;
                case UA_NODECLASS_DATATYPE:
                    puts("DataType");
                    break;
                case UA_NODECLASS_VIEW:
                    puts("View");
                    break;
                default:
                    puts("Sconosciuto");
            }

            puts("");
        }
    }

    UA_BrowseRequest_clear(&request);
    UA_BrowseResponse_clear(&response);
}

void read_variable(UA_Client *client)
{
    char *node_id_string = NULL;

    UA_ReadRequest request;
    UA_ReadResponse response;
    UA_ReadValueId id;

    UA_ReadRequest_init(&request);
    UA_ReadValueId_init(&id);

    id.attributeId = UA_ATTRIBUTEID_VALUE;
    input_node_id(&(id.nodeId), &node_id_string);

    request.timestampsToReturn = UA_TIMESTAMPSTORETURN_SOURCE;
    request.nodesToRead = &id;
    request.nodesToReadSize = 1;

    response = UA_Client_Service_read(client, request);

    if (response.responseHeader.serviceResult == UA_STATUSCODE_GOOD &&
        response.resultsSize >= 1) {
        print_datavalue(response.results);
    } else {
        puts("Lettura non riuscita");
    }

    if (node_id_string != NULL) {
        free(node_id_string);
    }

    UA_ReadResponse_clear(&response);
}

void read_namespace_array(UA_Client *client)
{
    char *node_id_string = NULL;

    UA_ReadRequest request;
    UA_ReadResponse response;
    UA_ReadValueId id;

    UA_ReadRequest_init(&request);
    UA_ReadValueId_init(&id);

    id.attributeId = UA_ATTRIBUTEID_VALUE;
    id.nodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_SERVER_NAMESPACEARRAY);

    request.nodesToRead = &id;
    request.nodesToReadSize = 1;

    response = UA_Client_Service_read(client, request);

    if (response.responseHeader.serviceResult == UA_STATUSCODE_GOOD &&
        response.resultsSize >= 1) {
        if (!response.results[0].hasValue ||
            response.results[0].value.type != &UA_TYPES[UA_TYPES_STRING]) {
            puts("Valore non valido");
        } else {
            printf("Namespace array: ");
            print_variant(&(response.results[0].value));
        }
    } else {
        puts("Lettura non riuscita");
    }

    if (node_id_string != NULL) {
        free(node_id_string);
    }

    UA_ReadResponse_clear(&response);
}

void monitor_data_change(UA_Client *client, UA_UInt32 subscription_id,
                         void *sub_context, UA_UInt32 monitor_id,
                         void *monitor_context, UA_DataValue *value) 
{
    printf("Cambiamento del valore del monitored item %d sulla subscription %d\n",
            monitor_id, subscription_id);
    print_datavalue(value);
}

// Eliminazione dell'oggetto DataChangeFilter precedentemente creato
void monitor_cleanup(UA_Client *client, UA_UInt32 subscription_id,
                     void *sub_context, UA_UInt32 monitor_id,
                     void *monitor_context)
{
    UA_delete(monitor_context, &UA_TYPES[UA_TYPES_DATACHANGEFILTER]);
}

void create_monitored_item(UA_Client *client, UA_UInt32 subscription_id)
{
    char *node_id_string = NULL, buffer[100];
    int len = 0;
    UA_NodeId node_id;
    UA_DataChangeFilter *filter = UA_new(&UA_TYPES[UA_TYPES_DATACHANGEFILTER]);

    UA_MonitoredItemCreateRequest request;
    UA_MonitoredItemCreateResult result;

    input_node_id(&node_id, &node_id_string);

    request = UA_MonitoredItemCreateRequest_default(node_id);
    request.monitoringMode = UA_MONITORINGMODE_REPORTING;

    for (;;) {
        printf("Sampling Interval [DEFAULT: %.2f]: ", request.requestedParameters.samplingInterval);
        if (input_text(sizeof(buffer), buffer) == 0 ||
            sscanf(buffer, "%lf", &request.requestedParameters.samplingInterval) > 0) {
            break;
        }
    }

    for (;;) {
        printf("Queue Size [DEFAULT: %d]: ", request.requestedParameters.queueSize);
        if (input_text(sizeof(buffer), buffer) == 0 ||
            sscanf(buffer, "%d", &request.requestedParameters.queueSize) > 0) {
            break;
        }
    }

    request.requestedParameters.discardOldest = input_sn("Discard Oldest");

    if (input_sn("Vuoi impostare un filtro?")) {
        for (;;) {
            printf("Deadband (1.00 -> Absolute / 0.5%% -> Percent): ");
            len = input_text(sizeof(buffer), buffer);
            if (len > 0 && sscanf(buffer, "%lf", &(filter->deadbandValue)) > 0) {
                if (buffer[len - 1] == '%') {
                    filter->deadbandType = UA_DEADBANDTYPE_PERCENT;
                } else {
                    filter->deadbandType = UA_DEADBANDTYPE_ABSOLUTE;
                }
                break;
            }
        }

        filter->trigger = UA_DATACHANGETRIGGER_STATUSVALUE;

        request.requestedParameters.filter.encoding = UA_EXTENSIONOBJECT_DECODED;
        request.requestedParameters.filter.content.decoded.data = filter;
        request.requestedParameters.filter.content.decoded.type = &UA_TYPES[UA_TYPES_DATACHANGEFILTER];
    }

    result = UA_Client_MonitoredItems_createDataChange(
        client, subscription_id, UA_TIMESTAMPSTORETURN_SERVER,
        request, filter, monitor_data_change, monitor_cleanup
    );

    if (result.statusCode != UA_STATUSCODE_GOOD) {
        puts("Creazione non riuscita");
    } else {
        printf("Creato il monitored item %d sulla subscription %d\n",
                result.monitoredItemId, subscription_id);
    }

    if (node_id_string != NULL) {
        free(node_id_string);
    }
    
}

void create_subscription(UA_Client *client)
{
    UA_CreateSubscriptionRequest request = UA_CreateSubscriptionRequest_default();
    UA_CreateSubscriptionResponse response;
    char buffer[100];

    for (;;) {
        printf("Requested Publishing Interval [DEFAULT: %.2f]: ", request.requestedPublishingInterval);
        if (input_text(sizeof(buffer), buffer) == 0 ||
            sscanf(buffer, "%lf", &request.requestedPublishingInterval) > 0) {
            break;
        }
    }

    for (;;) {
        printf("Requested Max Keep Alive Count [DEFAULT: %d]: ", request.requestedMaxKeepAliveCount);
        if (input_text(sizeof(buffer), buffer) == 0 ||
            sscanf(buffer, "%d", &request.requestedMaxKeepAliveCount) > 0) {
            break;
        }
    }

    for (;;) {
        request.requestedLifetimeCount = request.requestedMaxKeepAliveCount * 1000;

        printf("Requested Lifetime Count [DEFAULT: %d]: ", request.requestedLifetimeCount);
        if (input_text(sizeof(buffer), buffer) == 0 ||
            (sscanf(buffer, "%d", &request.requestedLifetimeCount) > 0 &&
             request.requestedLifetimeCount > request.requestedMaxKeepAliveCount * 3)) {
            break;
        }
    }

    for (;;) {
        printf("Maximum Number of Notifications in a Single Publish response [DEFAULT: %d]: ",
               request.maxNotificationsPerPublish);
        if (input_text(sizeof(buffer), buffer) == 0 ||
            sscanf(buffer, "%d", &request.maxNotificationsPerPublish) > 0) {
            break;
        }
    }

    response = UA_Client_Subscriptions_create(client, request, NULL, NULL, NULL);

    if (response.responseHeader.serviceResult == UA_STATUSCODE_GOOD) {
        printf("SubscriptionId: %d\n", response.subscriptionId);
        printf("Revised Publishing Interval: %f\n", response.revisedPublishingInterval);
        printf("Revised Lifetime Count: %d\n", response.revisedLifetimeCount);
        printf("Revised Max Keep Alive Count: %d\n", response.revisedMaxKeepAliveCount);

        while (input_sn("Vuoi aggiungere un Monitored Item?")) {
            create_monitored_item(client, response.subscriptionId);
        }
    } else {
        puts("Creazione della subscription non riuscita");
    }
}

void *client_loop(void *client_data)
{
    for (;;) {
        #ifdef PTHREAD
        pthread_mutex_lock(&((client_data_t *) client_data)->lock);
        if (((client_data_t *) client_data)->close) {
            break;
        }
        #endif
        UA_Client_run_iterate(((client_data_t *) client_data)->client, 1000);
        #ifdef PTHREAD
        pthread_mutex_unlock(&((client_data_t *) client_data)->lock);
        #endif
        UA_sleep_ms(1000);
    }
    return NULL;
}
    
int main(int argc, char **argv)
{
    UA_Client *client = client_connect(argc > 1 ? argv[1] : NULL);
    client_data_t client_data;
    int close = 0, choice;
    char *msg_command[] = {
        "Browse",
        "Lettura variabili",
        "Lettura namespace array",
        "Creazione subscription",
        #ifndef PTHREAD
        "Client loop (necessario per ricevere le notification)",
        #endif
        "Chiusura"
    };
    #ifdef PTHREAD
    pthread_t thread;
    #endif

    if (!client) {
        return 1;
    }

    client_data.client = client;

    #ifdef PTHREAD
    client_data.close = 0;
    pthread_mutex_init(&(client_data.lock), NULL);

    if (pthread_create(&thread, NULL, client_loop, (void *) &client_data) != 0) {
        puts("Creazione del thread per il client non riuscita");
    }
    #endif

    while (!close) {
        puts("");
        puts("Cosa fare?");
        choice = input_choice(sizeof(msg_command) / sizeof(char*), msg_command);
        #ifdef PTHREAD
        pthread_mutex_lock(&(client_data.lock));
        #endif
        switch (choice) {
            case 0:
                browse(client);
                break;

            case 1:
                read_variable(client);
                break;

            case 2:
                read_namespace_array(client);
                break;

            case 3:
                create_subscription(client);
                break;

            #ifndef PTHREAD
            case 4:
                client_loop((void *) &client_data);
                break;
            #endif

            default:
                close = 1;
        }
        #ifdef PTHREAD
        pthread_mutex_unlock(&(client_data.lock));
        #endif
    }

    #ifdef PTHREAD
    client_data.close = 1;
    pthread_mutex_destroy(&(client_data.lock));
    pthread_join(thread, NULL);
    #endif

    UA_Client_delete(client);

    return 0;
}
