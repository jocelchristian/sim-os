max_processes :: 100
max_events_per_process :: 10
max_single_event_duration :: 10
max_arrival_time :: 7

for 0..10 {
    spawn_random_process()
}
