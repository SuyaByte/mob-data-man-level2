    /*Project #3;*/

    #include "csim.h"
    #include <stdio.h>

    #define SIMTIME 2000.0
    #define NUMNODES 5
    #define DBSIZE 500
    #define BANDWIDTH 10000
    #define CACHESIZE 100
    #define EVENT_OCCURED 4l
    #define TIMED_OUT -1l
    #define TIME_OUT 20.0
    #define REQUEST 0l
    #define DATA 1l
    #define CHECK 2l
    #define ACK 3l
    #define T_QUERY 10
    #define T_UPDATE 5
    #define HOTDATA 50

    typedef struct msg *msg_t;

    struct msg {
        long type;//2bits
        long from;//3bits
        long to;//3bits
        int itemid;//9bits
        TIME last_update_time;//16bits
        TIME last_access_time;//16bits
        int size;//14bits
        TIME sent_time;//16bits
        msg_t next;
    };

    typedef struct databse {
        int id;
        TIME last_update_time;
        TIME last_access_time;
        int size;
    } databse_t;


    typedef struct nde {
        databse_t cache[CACHESIZE];
        FACILITY cpu;
        MBOX input;
    } nde_t;

    typedef struct srvr {
        FACILITY cpu;
        MBOX input;
    } srvr_t;

    FACILITY network[NUMNODES];
    nde_t node[NUMNODES];
    databse_t db[DBSIZE];
    srvr_t servr;
    int queries[5] = {0,0,0,0,0};
    int hits[5] = {0,0,0,0,0};
    float delay[5] = {0,0,0,0,0};
    EVENT done;		
    TABLE tbl;		
    msg_t mssg;
    void create_db();
    void server();
    void client();
    void data_update();
    int check_local_cache();
    void init();
    msg_t new_msg();
    void send_msg();
    void form_data_msg();
    void form_ack_msg();
    void cache_update();
    void return_msg();
    void create_cache();

    void sim()
    {
        set_model_name("Pull-Based Cache Invalidation");
        create("sim");	
        create_db();
        data_update();
        server();
        init();
        hold(SIMTIME);
        report();
        report_mailboxes();
        mdlstat();
    }

    //process to create the database
    void create_db()
    {
        int i;
        create("database");
        for(i=0; i<DBSIZE; i++)
        {
            db[i].id = i;
            db[i].last_update_time = clock;
            db[i].last_access_time = clock;
            db[i].size = 8192;
        }
    }

    void create_cache(long i)
    {
        int j;
        char str[6];
        sprintf(str, "Cache%d", i);
        create(str);
        for(j=0; j<=CACHESIZE; j++)
        {
            db[j].id = -1;
        }
    }

    void data_update()
    {
        int temp;
        float update_prob;
        long t;
        create("updates");
        while(clock < SIMTIME)
        {
            t = exponential(T_UPDATE);
            hold(t);
            update_prob = uniform(0, 1);
            if(update_prob <= 0.33)
            {
                temp = uniform(0, HOTDATA-1);
                db[temp].last_update_time = clock;
                printf("Data Item %d is updated at simulation time:%6.3f\n", temp, db[temp].last_update_time);
            }
            else
            {
                temp = uniform(HOTDATA, DBSIZE-1);
                db[temp].last_update_time = clock;
                printf("Data Item %d is updated at simulation time:%6.3f\n", temp, db[temp].last_update_time);
            }
        }
        if(clock > SIMTIME)
        {
            set(done);
        }
    }

    void server()
    {
        msg_t ms, m1;
        long t, i, results;
        TIME cache_time;
        int id;
        int sz;
        TIME update_time;
        TIME access_time;
        create("server");
        servr.input = mailbox("server_mb");
        servr.cpu = facility("server_cpu");
        while(clock < SIMTIME)
        {
            results = timed_receive(servr.input, (long *)&m1, TIME_OUT);
            
            switch(results)
            {
                case TIMED_OUT:      
                        printf("Time out occured at the server\n");
                        break;
                
                case EVENT_OCCURED:   
                        t = m1->type;
                        id = m1->itemid;
                        i = m1->from;
                        sz = db[id].size;
                        update_time = db[id].last_update_time;
                        if(t == REQUEST)
                        {
                            form_data_msg(m1, i, id, sz, update_time);
                            send_msg(m1);
                            printf("Data Item %d is accessed from the DB --> Server sends data msg to client %ld\n", id, i);
                            cache_time = m1->last_update_time;
                            break;
                        }
                        else if(t == CHECK)
                        {
                            cache_time = m1->last_update_time;
                            if(db[id].last_update_time > cache_time)
                            {
                                printf("Data Item %d is updated --> server sends data msg to client %ld\n", id, i);
                                t == DATA;
                                form_data_msg(m1, i, id, sz, update_time);
                                update_time = m1->last_update_time;
                                send_msg(m1);
                            }
                            else
                            {
                                printf("Data Item %d is valid --> server sends ack msg to client %ld\n", id, i);
                                form_ack_msg(m1);
                                send_msg(m1);
                            }
                        }
                        break;
                default:
                break;
            }
        } 
    }

    void init()            
    {
        long i;
        float total1, total2;
        int totalMessages;
        char str[10];
        for(i=0; i<NUMNODES; i++)
        {
            sprintf(str, "channel%d", i);
            network[i] = facility(str);
            sprintf(str, "m%d", i);
            node[i].input = mailbox(str);
            sprintf(str, "cpu%d", i);
            node[i].cpu = facility(str);
        }
        
        done = event("done");  
        tbl = table("Response Times");	           
        for(i=0; i<NUMNODES; i++)
        {
            create_cache(i);
            client(i);
        }

        wait(done);
        for(i=0; i<NUMNODES; i++)
            {   
                printf("\nTotal number of queries generated by Client.%ld are %ld.", i, queries[i]);
                printf("\nTotal number of cache hits by Client.%ld are %ld.", i, hits[i]);
                printf("\nQuery delay of Client.%ld is %f", i, delay[i]);
            }
        printf("\n\n==================================================");
        total1 = (queries[0]+queries[1]+queries[2]+queries[3]+queries[4])/5;
        printf("\nAverage number of queries are %.2f.\n", total1);
        total2 = (hits[0]+hits[1]+hits[2]+hits[3]+hits[4])/5;
        printf("\nAverage number of cache hits are %.2f.\n",total2);
        printf("\nCache Hit ratio is %.2f.\n",(float)total2/total1);
        printf("\nAverage query delay is %.2f.\n",(float)(((total1-total2)*0.8271)+(0.79*2*total2)));
        printf("==================================================\n\n");
    } 

    void client(long i)
    {
        int temp,cache_index, cache_id, cache_size, cache_status;
        float generate_prob;
        long t, set_type,results,type;
        msg_t m1,m2;
        TIME tmstmp, new_update_time,cache_update_time, cache_access_time,time,tim;
        create("client");
        while(clock < SIMTIME)
        {   
            results = timed_receive(node[i].input, (long *)&m1, TIME_OUT);
            switch(results)
            {
                case TIMED_OUT:    
                        t = exponential(T_QUERY);
                        hold(t); 
                        generate_prob = uniform(0, 1);
                        if(generate_prob <= 0.8)
                        {
                            temp = uniform(0, HOTDATA-1);
                        }
                        else
                        {
                            temp = uniform(HOTDATA,DBSIZE-1);
                        }    
                        printf("Client %ld generated a query for Data Item %d at simulation time:%6.3f\n", i, temp, clock);
                        cache_index = check_local_cache(i, temp); //should return 0 if not cached 1 if cached//must include i, temp for more nodes
                        if(cache_index != -1)
                        {
                            node[i].cache[cache_index].last_access_time = clock;
                            printf("Data Item %d is cached at client %ld --> sending check msg to server\n", temp, i);
                            tmstmp = node[i].cache[cache_index].last_update_time;
                            set_type = CHECK;
                            tim = clock;
                            m1 = new_msg(i, temp, tmstmp, set_type, tim);
                            send_msg(m1);
                            queries[i]++;
                        }
                        else
                        {
                            printf("Data Item %d is not cached at Client %ld --> sending request msg to server\n", temp, i);
                            tmstmp = 0;
                            set_type = REQUEST;
                            tim = clock;
                            m1 = new_msg(i, temp, tmstmp, set_type, tim);
                            send_msg(m1);
                            queries[i]++;
                        }                  
                                
                case EVENT_OCCURED:   
                        type = m1->type;
                        time = clock;
                        delay[i] = (delay[i] + time)/2;
                        if(type == ACK)
                        {
                            printf("The cached copy of data item %d at client %d is valid\n", temp, i);
                            hits[i]++;
                            record(clock - m1->sent_time, tbl);
                            return_msg(m1);
                            break;
                        }
                        else
                        {
                            if(cache_index != -1)
                            {
                                new_update_time = m1->last_update_time;
                                cache_update(i, cache_index, new_update_time);
                                printf("The cached copy of data item %d at client %d is invalid. Updating the cached data item\n", temp, i);
                                return_msg(m1);
                            }
                            else
                            {
                                cache_id = m1->itemid;
                                cache_update_time = m1->last_update_time;
                                cache_access_time = clock;
                                cache_size = m1->size;
                                cache_status = caching(i, cache_id, cache_update_time, cache_access_time, cache_size);
                                if(cache_status != -1)
                                    {
                                        if(cache_status == CACHESIZE-1)
                                        {
                                            queries[i]=0;
                                            hits[i]=0;
                                        }
                                        printf("The data item %d is now cached at client %d\n", temp, i);
                                    }
                                    else
                                    {
                                        cache_replacement(i, cache_id, cache_update_time, cache_access_time, cache_size);
                                        printf("Cache is full. Implementing LRU cache replacement policy\n");
                                    }
                                return_msg(m1);
                            }
                            
                        break; 
                        }
                default:
                break;
            }
        }
        if(clock>=SIMTIME)
        {
            set(done);
        }
    }

    int check_local_cache(i, temp)
    {
        int j;
        for(j=0; j<CACHESIZE; j++)
        {
            if(node[i].cache[j].id == temp)
            {
                return j;
            }
        }
        return -1;
    }

    cache_replacement(i, cache_id, cache_update_time, cache_access_time, cache_size)
    {
        TIME oldest = node[i].cache[0].last_access_time;
        int j, oldest_index;
        for(j=0; j<CACHESIZE; j++)
        {
            if(node[i].cache[j].last_access_time < oldest)
            {
                oldest = node[i].cache[j].last_access_time;
                oldest_index = j;
            }
        }
        node[i].cache[oldest_index].id = cache_id;
        node[i].cache[oldest_index].last_update_time = cache_update_time;
        node[i].cache[oldest_index].last_access_time = cache_access_time;
        node[i].cache[oldest_index].size = cache_size;
    }

    int caching(i, cache_id, cache_update_time, cache_access_time, cache_size)
    {
        int j;
        for(j=0; j<CACHESIZE; j++)
        {
            //added this part
            if(node[i].cache[j].id == cache_id)
            {
                node[i].cache[j].last_update_time = cache_update_time;
                node[i].cache[j].last_access_time = cache_access_time;
                node[i].cache[j].size = cache_size;
                return j;
            }
            //
            else if(node[i].cache[j].id == 0)
            {
                node[i].cache[j].id = cache_id;
                node[i].cache[j].last_update_time = cache_update_time;
                node[i].cache[j].last_access_time = cache_access_time;
                node[i].cache[j].size = cache_size;
                return j;
            }
        }
        return -1;
    }

    void return_msg(m)
    msg_t m;
    {
        m->next = mssg;
        mssg = m;
    }

    msg_t new_msg(from, item_id, time_stamp, typeofmsg, tim)
    long from; int item_id; TIME time_stamp; long typeofmsg; TIME tim;
    {
        msg_t m;
        long a;
        a = typeofmsg;
        if(mssg == NIL)
        {
            m = (msg_t)do_malloc(sizeof(struct msg));
        }
        else
        {
            m = mssg;
            mssg = mssg -> next;
        }
        m->type = typeofmsg;
        m->from = from;
        m->to = 6;
        m->itemid = item_id;
        m->last_update_time = time_stamp;
        m->last_access_time = 0;
        m->size = 0;
        m->sent_time = tim;
        return(m);
    }

    void send_msg(msg_t from_node)
    {
        long from, type, hold_time, to;
        from = from_node -> from;
        type = from_node -> type;
        to = from_node -> to;
        if(type == DATA)
        {
            hold_time = (8192+79)/(float)10000;
        }
        else
        {
            hold_time = 79/(float)10000;
        }
        if(to == 6)
        {
            reserve(network[from]);
            hold(hold_time);
            send(servr.input, (long) from_node);
            release(network[from]); 
        }
        else
        {
            reserve(network[to]);
            hold(hold_time);
            send(node[to].input, (long) from_node);
            release(network[to]);
        }
    }

    void form_data_msg(msg_t m,long i, int id,int sz, TIME update_time)
    {
        long from, to;
        from = m->from;
        to = m->to;
        m->from = to;
        m->to = from;
        m->type = DATA;
        m->itemid = id;
        m->last_update_time = update_time;
        m->size = sz;
    }

    void form_ack_msg(m)
    msg_t m;
    {
        long from, to;
        from = m->from;
        to = m->to;
        m->from = to;
        m->to = from;
        m->type = ACK;
    }

    void cache_update(long i,int cache_index,TIME new_update_time)
    {
        node[i].cache[cache_index].last_update_time = new_update_time;
    }