#include <uuid/uuid.h>
#include <spdlog/spdlog.h>
#include <MQTTClient.h>
#include <structopt/app.hpp>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <thread>
#include <string>
#include <vector>

struct Options
{
    std::string address;
    std::string topic;
    std::optional<int> qos = 1;
    std::optional<bool> verbose = false;
};
STRUCTOPT(Options, address, topic, qos, verbose);
Options option;

std::mutex mqtt_message_mutex;
int mqtt_message_received = 0;
std::string mqtt_message_payload;

std::string generate_uuid_string()
{
    uuid_t uuid;
    uuid_generate(uuid);
    char uuid_str[37];
    uuid_unparse(uuid, uuid_str);
    return std::string{uuid_str};
}

void delivered(void *context, MQTTClient_deliveryToken dt)
{
    static volatile MQTTClient_deliveryToken mqtt_deliveredtoken;
    printf("Message with token value %d delivery confirmed\n", dt);
    mqtt_deliveredtoken = dt;
}

int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
    {
        std::unique_lock lk(mqtt_message_mutex);
        mqtt_message_received++;
        mqtt_message_payload = (char *)message->payload;
        if (option.verbose.value())
            spdlog::info("Message {} arrived, length: {}.\n{}", mqtt_message_received, message->payloadlen, (char *)message->payload);
        else
            spdlog::info("Message {} arrived, length: {}.", mqtt_message_received, message->payloadlen);
    }

    // printf("   topic: %s\n", topicName);
    // printf("   message: %.*s\n", message->payloadlen, (char *)message->payload);
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

void connlost(void *context, char *cause)
{
    printf("\nConnection lost\n");
    printf("     cause: %s\n", cause);
}

int main(int argc, char *argv[])
{
    try
    {
        option = structopt::app("mqtt-inspector").parse<Options>(argc, argv);
    }
    catch (structopt::exception &e)
    {
        std::cout << e.what() << "\n";
        std::cout << e.help();
        exit(EXIT_FAILURE);
    }

    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    std::string client_id = generate_uuid_string();

    if (int rc; (rc = MQTTClient_create(&client, option.address.c_str(), client_id.c_str(),
                                        MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS)
    {
        spdlog::error("failed to create client, return code {}", rc);
        exit(EXIT_FAILURE);
    }
    else
    {
        spdlog::info("mqtt client created successfully.");
    }

    if (int rc; (rc = MQTTClient_setCallbacks(client, NULL, connlost, msgarrvd, delivered)) != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to set callbacks, return code %d\n", rc);
        goto FAIL_ON_ERROR;
    }

    if (int rc; (rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
        spdlog::error("Failed to connect, return code {}", rc);
        goto FAIL_ON_ERROR;
    }
    else
    {
        spdlog::info("mqtt client '{}' connect to mqtt broker: '{}'", client_id, option.address);
    }

    if (int rc; (rc = MQTTClient_subscribe(client, option.topic.c_str(), option.qos.value())) != MQTTCLIENT_SUCCESS)
    {
        spdlog::error("Failed to subscribe, return code {}", rc);
        goto FAIL_ON_ERROR;
    }
    else
    {
        spdlog::info("mqtt client subscribe on topic '{}' successfully.", option.topic);

        int ch;
        do
        {
            sleep(1);
            ch = getchar();
        } while (ch != 'Q' && ch != 'q');

        if ((rc = MQTTClient_unsubscribe(client, option.topic.c_str())) != MQTTCLIENT_SUCCESS)
        {
            spdlog::error("Failed to unsubscribe, return code {}", rc);
            goto FAIL_ON_ERROR;
        }
        else
        {
            spdlog::info("mqtt client '{}' unsubscribe on topic '{}'", client_id, option.topic);
        }
    }

    if (int rc; (rc = MQTTClient_disconnect(client, 10000)) != MQTTCLIENT_SUCCESS)
    {
        spdlog::error("Failed to disconnect, return code %d\n", rc);
        goto FAIL_ON_ERROR;
    }
    else
    {
        spdlog::info("mqtt client '{}' disconnect successfully.", client_id);
    }

    MQTTClient_destroy(&client);
    return EXIT_SUCCESS;
FAIL_ON_ERROR:
    MQTTClient_destroy(&client);
    exit(EXIT_FAILURE);
}