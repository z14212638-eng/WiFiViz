# wifi-txop-aggregation

This is an example that illustrates how 802.11n aggregation is configured. It defines 4 independent Wi-Fi networks (working on different channels). Each network contains one access point and one station. Each station continuously transmits data packets to its respective AP.

Network topology (numbers in parentheses are channel numbers):

- Network A (36): AP A <-> STA A
- Network B (40): AP B <-> STA B 
- Network C (44): AP C <-> STA C
- Network D (48): AP D <-> STA D

The aggregation parameters are configured differently on the 4 stations:

- Station A uses default aggregation parameter values (A-MSDU disabled, A-MPDU enabled with maximum size of 65 kB).
- Station B doesn't use aggregation (both A-MPDU and A-MSDU are disabled).
- Station C enables A-MSDU (with maximum size of 8 kB) but disables A-MPDU.
- Station D uses two-level aggregation (A-MPDU with maximum size of 32 kB and A-MSDU with maximum size of 4 kB).

The user can select the distance between the stations and the APs, can enable/disable the RTS/CTS mechanism and can modify the duration of a TXOP.

Example:

```
./ns3 run "wifi-txop-aggregation --distance=10 --enableRts=0 --simulationTime=20s"
```

The output prints the throughput and the maximum TXOP duration measured for the 4 cases/networks described above. When default aggregation parameters are enabled, the maximum A-MPDU size is 65 kB and the throughput is maximal. When aggregation is disabled, the throughput is about the half of the physical bitrate. When only A-MSDU is enabled, the throughput is increased but is not maximal, since the maximum A-MSDU size is limited to 7935 bytes (whereas the maximum A-MPDU size is limited to 65535 bytes). When A-MSDU and A-MPDU are both enabled (= two-level aggregation), the throughput is slightly smaller than the first scenario since we set a smaller maximum A-MPDU size.

