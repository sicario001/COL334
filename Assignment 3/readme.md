1. To generate the plots for part 1, place the First.cc file in scratch folder and create an output folder in the ns-3.29 directory and use the command `./waf --run "scratch/First 0 protocol" > output/result.txt 2>&1`.
2. The protocols could be `Newreno`, `Highspeed`, `Veno` or `Vegas`.
3. To generate the output for the dropped packets and their count, use the command `./waf --run "scratch/First 1 output/result.txt"`.
4. The above command generates a file `droppedPackets.txt`, which contains the details regarding the dropped packets.
5. To generate the plots for part 2(a), place the Second.cc file in scratch folder and create an output folder in the ns-3.29 directory and use the command `./waf --run "scratch/Second 0 channelDatarate"`. To generate the plots for part 2(b), use the command `./waf --run "scratch/Second 1 applicationDatarate"`.
6. Use command `./waf --run "scratch/Third <configuration>"` for part 3, to run the simulation and generate plots in the required configuration mode.
7. Use command `tcpick -C -yP -r PhyRxDrop.pcap` to get the number of packets dropped from the pcap file.
