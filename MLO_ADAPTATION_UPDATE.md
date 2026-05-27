# MLO Adaptation Update

This update aligns WiFiViz with ns-3 multi-link operation (MLO) traces and adds
MLO-aware visualization paths without removing the original PPDU timeline.

## Trace Model

- Shared-memory PPDU records now carry `nodeId`, `linkId`, `channelId`, and
  device role metadata.
- Device role records distinguish AP and STA devices, and PPDU/PHY state records
  propagate that role so MLO link addresses do not appear as generic devices.
- PHY state records are emitted per `(node, link, channel)` radio context.

## Timeline Views

The timeline mode button now cycles through:

1. PPDU timeline grouped by sender.
2. MLO PPDU timeline grouped by node/link.
3. MLO channel state timeline grouped by node/link/channel.
4. Aggregate channel state timeline grouped by channel.
5. PHY state timeline grouped by node/link.

For a two-link AP/STA MLO scenario on channels 1 and 36, the MLO channel state
timeline shows four rows:

- `AP 0 / L0 / CH1`
- `AP 0 / L1 / CH36`
- `STA 1 / L0 / CH1`
- `STA 1 / L1 / CH36`

This reflects the MLD-level AP and STA devices while preserving each link's
independent channel state.

## Metrics And Filters

- Node filter labels now resolve to `AP <nodeId>` or `STA <nodeId>` when role
  metadata is available.
- Link filters remain available for MLO charts and delay metrics.
- The PHY State Share pie chart now sums PHY state durations across the selected
  node/link scope. It no longer reports an average per PHY.

## Performance

- PHY state timeline rendering now caches row layout and sorted state segments.
- Drawing and hover handling only scan the visible time range.
- Dense short PHY state segments are batched to reduce painter overhead.
- Incoming high-rate PHY state records are throttled for UI refreshes while
  preserving the recorded data.

## Compatibility

- Existing single-link PPDU timeline behavior remains available.
- The aggregate channel state view is retained for channel-level inspection.
- The original PPDU timeline and the new MLO views share the same data stream.
