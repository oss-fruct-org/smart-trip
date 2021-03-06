#include <stdlib.h>
#include <stdio.h>
#include <locale.h>
#include <pthread.h>

#include <smartslog.h>

#include "ontology.h"
#include "common.h"

// Exported to JNA
typedef struct {
    int count;
    double* points;
    
    char* user_id;
    double user_lat;
    double user_lon;

    char* tsp_type;
    char* road_type;

    sslog_individual_t** point_individuals;
    sslog_individual_t* route;
} RequestData;

bool init(const char* name, const char* smartspace, const char* address, int port);
void shutdown();

bool subscribe();
void unsubscribe();

RequestData* wait_subscription(void);
void publish(int points_count, int* ids, double* weights, const char* roadType, RequestData* requestData); 

static sslog_node_t* node;
static sslog_subscription_t* sub;

static PtrArray requests_array;
static pthread_mutex_t requests_mutex = PTHREAD_MUTEX_INITIALIZER;
// static pthread_cond_t requests_cond = PTHREAD_COND_INITIALIZER;


/**
 * Инициализаия нативной части модуля
 * @param name Имя модуля
 * @param smartspace Имя ИП
 * @param address Адрес ИП
 * @param port Порт ИП
 * @return true если инициализация прошла успешно, иначе false
 */
bool init(const char* name, const char* smartspace, const char* address, int port) {
    setlocale(LC_NUMERIC, "C");
    init_rand();

    sslog_init();
    register_ontology();

    node = create_node_resolve(name, smartspace, address, port);

    if (node == NULL || sslog_node_join(node) != SSLOG_ERROR_NO) {
        return false;
    } else {
        return true;
    }
}

/**
 * Отключение от ИП. Необходимо вызывать перед закрытием модуля.
 */
void shutdown() {
    unsubscribe();
    if (node != NULL)
        sslog_node_leave(node);
    node = NULL;
    sslog_shutdown();
}

/**
 * Получение данных по заявке на маршрут
 * @param user Индивид пользователя (устарело) или null
 * @param schedule Индивид расписания (устарело) или null
 * @param route Индивид маршрута
 */
static void handle_updated_request(sslog_individual_t* user, sslog_individual_t* schedule, sslog_individual_t* route) {
    fprintf(stderr, "%s:%i: handle_updated_request %s %s %s\n", __FILE__, __LINE__,
            sslog_entity_get_uri(user),
            sslog_entity_get_uri(schedule),
            sslog_entity_get_uri(route));

    CLEANUP_INDIVIDUAL sslog_individual_t* location;
    if (user != NULL) {
        location = (sslog_individual_t*) sslog_node_get_property(node, user, PROPERTY_HASLOCATION);
    } else {
        location = (sslog_individual_t*) sslog_node_get_property(node, route, PROPERTY_HASLOCATION);
    }
    const char* user_lat_str = NULL;
    const char* user_lon_str = NULL;
    if (location != NULL) {
        sslog_node_populate(node, location);
        user_lat_str = sslog_get_property(location, PROPERTY_LAT);
        user_lon_str = sslog_get_property(location, PROPERTY_LONG);
    }

    if (location == NULL || user_lat_str == NULL || user_lon_str == NULL) {
        fprintf(stderr, "User location is NULL or incomplete\n");
    }


    // Clean local stored points
    // This is required, because populate doesn't remove local properties, that was removed in sib
    sslog_remove_properties(route, PROPERTY_HASPOINT);
    sslog_remove_properties(route, PROPERTY_TSPTYPE);
    sslog_remove_properties(route, PROPERTY_ROADTYPE);
    sslog_node_populate(node, route);

    list_t* points = sslog_get_properties(route, PROPERTY_HASPOINT);
    int count;

    if (points == NULL || (count = list_count(points)) == 0) {
        fprintf(stderr, "Route received but has no points\n");
        if (points != NULL) {
            list_free_with_nodes(points, NULL);
        }
        return;
    }

    RequestData* request_data = calloc(1, sizeof(RequestData));

    double* points_array = malloc(count * 2 * sizeof(double));
    sslog_individual_t** point_individuals = malloc(count * sizeof(sslog_individual_t*));
    
    int c = 0;
    list_head_t* iter;
    list_for_each(iter, &points->links) {
        list_t* entry = list_entry(iter, list_t, links);
        sslog_individual_t* point_individual = (sslog_individual_t*) entry->data;
        sslog_node_populate(node, point_individual);

        double lat, lon;
        if (!get_point_coordinates(node, point_individual, &lat, &lon)) {
            fprintf(stderr, "%s:%i: Can't get point coordinates\n", __FILE__, __LINE__);
            return;
        }

        //fprintf(stdout, "Point %lf %lf\n", lat, lon);

        points_array[2 * c] = lat;
        points_array[2 * c + 1] = lon;

        point_individuals[c] = point_individual;

        c += 1;
    }
    fprintf(stdout, "All points received\n");

    request_data->user_id = strdup(sslog_entity_get_uri(user));
    request_data->route = route;
    request_data->points = points_array;
    request_data->point_individuals = point_individuals;
    request_data->count = count;
    request_data->user_lat = user_lat_str != NULL ? parse_double(user_lat_str) : 0;
    request_data->user_lon = user_lat_str != NULL ? parse_double(user_lon_str) : 0;

    const char* tsp_type = sslog_get_property(route, PROPERTY_TSPTYPE);
    if (tsp_type != NULL) {
        request_data->tsp_type = strdup(tsp_type);
    }

    const char* road_type = sslog_get_property(route, PROPERTY_ROADTYPE);
    if (road_type != NULL) {
        request_data->road_type = strdup(road_type);
    }

    ptr_array_insert(&requests_array, request_data);
    fprintf(stdout, "%s:%i: Request enqueued (%li)\n", __FILE__, __LINE__, requests_array.size);

    list_free_with_nodes(points, NULL);
}

/**
 * Обработка события обновления маршрута (Route -> updated)
 * @param route_id
 */
static void handle_updated_property_update(const char* route_id) {
    fprintf(stdout, "handle_updated_property_update %s\n", route_id);
    sslog_individual_t* route_individual = sslog_node_get_individual_by_uri(node, route_id);
    // проверяем что маршрут это маршрут.
    sslog_triple_t *route_triple = sslog_individual_to_triple(route_individual);
    if (strcmp(route_triple->object, sslog_entity_get_uri(CLASS_ROUTE)) != 0) {
        fprintf(stdout, "%s:%i: Individe class %s is not a route\n", __FILE__, __LINE__, route_triple->subject);
        return;
    }
    
    sslog_individual_t* schedule_individual = st_get_subject_by_object(node, route_id, PROPERTY_HASROUTE);   

    if (schedule_individual == NULL) {
        fprintf(stdout, "%s:%i: Can't get schedule for updated route\n", __FILE__, __LINE__);
    }

    const char* schedule_uri = sslog_entity_get_uri(schedule_individual);
    fprintf(stdout, "schedule uri %s\n", schedule_uri);

    sslog_individual_t* user_individual = st_get_subject_by_object(node, schedule_uri, PROPERTY_PROVIDE);

    if (user_individual == NULL) {
        fprintf(stdout, "Can't get user for updated schedule\n");
    }

    handle_updated_request(user_individual, schedule_individual, route_individual);
}

static void handle_updated_property_location(const char* user_id, const char* location_individual) {
    fprintf(stdout, "handle_updated_property_location\n");
    sslog_individual_t* user_individual = sslog_node_get_individual_by_uri(node, user_id);
    sslog_individual_t* schedule_individual = (sslog_individual_t*) sslog_node_get_property(node, user_individual, PROPERTY_PROVIDE);
    sslog_individual_t* route_individual = (sslog_individual_t*) sslog_node_get_property(node, schedule_individual, PROPERTY_HASROUTE);

    if (!user_individual || !schedule_individual || !route_individual) {
        fprintf(stderr, "Can't handle location request\n");
        return;
    }

    handle_updated_request(user_individual, schedule_individual, route_individual);
}

static bool is_triple_updated_property(sslog_triple_t* triple) {
    return strcmp(triple->predicate, sslog_entity_get_uri(PROPERTY_UPDATED)) == 0;
}

static bool is_triple_location_property(sslog_triple_t* triple) {
    return strcmp(triple->predicate, sslog_entity_get_uri(PROPERTY_HASLOCATION)) == 0;
}

/**
 * Обработка события по подписке
 * @param sub указатель на подписку
 */
static void subscription_handler_2(sslog_subscription_t* sub) {
    fprintf(stdout, "subscription_handler_2\n");
    pthread_mutex_lock(&requests_mutex);

    sslog_sbcr_changes_t* changes = sslog_sbcr_get_changes_last(sub);
    const list_t* changes_list = sslog_sbcr_ch_get_triples(changes, SSLOG_ACTION_INSERT);

    list_head_t* iter;
    list_for_each (iter, &changes_list->links) {
        list_t* entry = list_entry(iter, list_t, links);
        sslog_triple_t* triple = entry->data;

        fprintf(stdout, "Triple received: <%s> <%s> <%s>\n", triple->subject, triple->predicate, triple->object);

        if (is_triple_updated_property(triple)) {
            handle_updated_property_update(triple->subject);
        }

        if (is_triple_location_property(triple)) {
            handle_updated_property_location(triple->subject, triple->object);
        }
    }

    pthread_mutex_unlock(&requests_mutex);
//    pthread_cond_signal(&requests_cond);
}

/**
 * Подписка на обновления в ИП.
 * @return true если подписка произошла успешно, иначе false
 */
bool subscribe() {
    if (node == NULL)
        return false;
    
    sub = sslog_new_subscription(node, false);
    sslog_sbcr_set_changed_handler(sub, &subscription_handler_2);

    sslog_triple_t* updated_triple = sslog_new_triple_detached(SSLOG_TRIPLE_ANY, sslog_entity_get_uri(PROPERTY_UPDATED), 
            SSLOG_TRIPLE_ANY, SSLOG_RDF_TYPE_URI, SSLOG_RDF_TYPE_LIT);
    sslog_sbcr_add_triple_template(sub, updated_triple);

    //sslog_triple_t* location_triple = sslog_new_triple_detached(SSLOG_TRIPLE_ANY, sslog_entity_get_uri(PROPERTY_HASLOCATION),
    //        SSLOG_TRIPLE_ANY, SSLOG_RDF_TYPE_URI, SSLOG_RDF_TYPE_URI);
//    sslog_sbcr_add_triple_template(sub, location_triple);

    ptr_array_init(&requests_array);
    // Ignore existing data, only process new schedule requests
    if (sslog_sbcr_subscribe(sub) != SSLOG_ERROR_NO) {
        return false;
    } else {
        return true;
    }
}

/**
 * Отключение подписки. Функция вызывается также в shutdown().
 */
void unsubscribe() {
    if (sub == NULL)
        return;
    
    sslog_sbcr_stop(sub);
    //sslog_sbcr_unsubscribe(sub);
    sslog_free_subscription(sub);
    ptr_array_free(&requests_array);
    sub = NULL;
}

/**
 * Получение последней полученной заявки из стека.
 * @return Заявка на построение маршрута или null если стек пуст.
 */
static RequestData* find_last_processed() {
    if (requests_array.size == 0)
        return NULL;

    RequestData* request_data = ptr_array_remove_last(&requests_array);
    return request_data;
}

/**
 * Основная функция. Ожидает получение сообщения по подписке и возвращает результат.
 * @return Заявка на построение маршрута или null если параметры заявки не удовлетворяют требованиям.
 */
RequestData* wait_subscription() {
    //pthread_mutex_lock(&requests_mutex);

    if (requests_array.size == 0) {
        if (sslog_sbcr_wait(sub) != SSLOG_ERROR_NO) {
            fprintf(stderr, "Error waiting subscription\n");
            return NULL;
        }

        if (!sslog_sbcr_is_active(sub)) {
            return NULL;
        }

        subscription_handler_2(sub);
    }

    RequestData* request_data = find_last_processed();
    fprintf(stdout, "%s:%i: Got request ptr %p\n", __FILE__, __LINE__, request_data);
    if (request_data == NULL) {
        fprintf(stderr, "Waiting for request\n");
        return NULL;
    }

    return request_data;
}

static void remove_old_movements_from_route(sslog_individual_t* route_individual) {
    list_t* individuals = sslog_get_properties(route_individual, PROPERTY_HASMOVEMENT);

    list_head_t* iter;
    list_for_each(iter, &individuals->links) {
        list_t* entry = list_entry(iter, list_t, links);
        sslog_individual_t* movement = entry->data;
        sslog_node_populate(node, movement);

        sslog_individual_t* start_point = (sslog_individual_t*) sslog_get_property(movement, PROPERTY_ISSTARTPOINT);
        sslog_individual_t* end_point = (sslog_individual_t*) sslog_get_property(movement, PROPERTY_ISENDPOINT);

        sslog_node_remove_individual_with_local(node, movement);
        sslog_node_remove_individual_with_local(node, start_point);
        sslog_node_remove_individual_with_local(node, end_point);
    }

    sslog_node_remove_property(node, route_individual, PROPERTY_HASMOVEMENT, NULL);
    sslog_node_remove_property(node, route_individual, PROPERTY_HASSTARTMOVEMENT, NULL);

    list_free_with_nodes(individuals, NULL);
}

void publish(int points_count, int* ids, double* weights, const char* roadType, RequestData* request_data) {
    fprintf(stderr, "Native publish\n");

    pthread_mutex_lock(&requests_mutex);
    int movements_count = points_count - 1;

    sslog_individual_t* route_individual = request_data->route;

    // remove_old_points_from_route(route_individual);
    remove_old_movements_from_route(route_individual);

    sslog_individual_t* point_individuals[points_count];
    sslog_individual_t* movement_individuals[movements_count];

    sslog_individual_t* user_point_individual = 
        create_poi_individual(node, request_data->user_lat, request_data->user_lon, "user-location", "system");

    for (int i = 0; i < points_count; i++) {
        int id = ids[i];
        if (id == -1) {
            point_individuals[i] = user_point_individual;
        } else {
            point_individuals[i] = request_data->point_individuals[id];
        }
    }

    sslog_individual_t* previous_movement = NULL;
    for (int i = 1; i < points_count; i++) {
        // These points already in smartspace
        sslog_individual_t* point1 = point_individuals[i - 1];
        sslog_individual_t* point2 = point_individuals[i];

        sslog_individual_t* movement = sslog_new_individual(CLASS_MOVEMENT, rand_uuid("movement"));
        sslog_insert_property(movement, PROPERTY_ISSTARTPOINT, point1);
        sslog_insert_property(movement, PROPERTY_ISENDPOINT, point2);
        sslog_insert_property(movement, PROPERTY_USEROAD, (void*) roadType);
        sslog_insert_property(movement, PROPERTY_LENGTH, double_to_string(weights[i - 1]));

        if (previous_movement != NULL) {
            sslog_node_insert_property(node, previous_movement, PROPERTY_HASNEXTMOVEMENT, movement);
        }

        sslog_node_insert_individual(node, movement);

        sslog_node_insert_property(node, route_individual, PROPERTY_HASMOVEMENT, movement);

        movement_individuals[i - 1] = movement;
        previous_movement = movement;
    }

    if (movements_count > 0) {
        fprintf(stderr, "Insert START_MOVEMENT property %s\n", sslog_entity_get_uri(movement_individuals[0]));
        sslog_node_insert_property(node, route_individual, PROPERTY_HASSTARTMOVEMENT, movement_individuals[0]);
    }

    sslog_node_remove_property(node, route_individual, PROPERTY_UPDATED, NULL);
    sslog_node_remove_property(node, route_individual, PROPERTY_PROCESSED, NULL);
    sslog_node_insert_property(node, route_individual, PROPERTY_PROCESSED, rand_uuid("processed"));

    pthread_mutex_unlock(&requests_mutex);

    free(request_data->points);
    free(request_data->user_id);
    free(request_data->tsp_type);
    free(request_data);
    
}
