# Project TODO

This backlog consolidates the ACK security, correctness, congestion and collision findings identified during static analysis of this source tree.

## Scope tags

- `ACK-ONLY`: The flaw or corrective work is specific to ACK production, matching, forwarding or ACK-driven application behaviour.
- `ALL-PACKETS`: The flaw exists in shared packet, routing, queueing, timing or resource-management machinery and can affect packet types beyond ACK.

Priorities use `P0` for remotely exploitable memory-safety or severe network-failure risks, `P1` for significant correctness, security or congestion defects, and `P2` for hardening, consistency, observability and testing.

## Design constraints

MeshCore operates over a high-latency, low-throughput radio network assembled by independent volunteers. Nodes have bounded memory and airtime, routes and membership change without notice, and no central authority can coordinate or enforce network-wide behaviour. Corrective work must preserve these properties:

- Flooding remains a bounded fallback when no usable route exists; route learning should reduce repeated floods, not assume they can be eliminated.
- Compact path identifiers necessarily permit occasional collisions; implementations can mitigate branching by selecting longer hashes where local density and packet space justify them.
- Retries and limited redundancy are required for unreliable links; the objective is to prevent recursive multiplication and obsolete transmissions, not to require single-copy delivery.
- Congestion, route progress, neighbour density and delivery state can only be estimated from local observations. Policies must not depend on exact global knowledge or trusted route-wide enforcement.
- Packet pools, queues, replay windows and deduplication histories must remain finite. Delivery and at-most-once guarantees therefore apply only within documented retention windows.
- Silence is inherently ambiguous: it cannot distinguish loss, delay, remote rejection or a lost response. Applications should expose confirmed, explicitly rejected and indeterminate outcomes and make retryable operations idempotent where practical.
- Priority is necessary for time-sensitive control traffic. Fairness work must bound starvation without treating every packet class equally or delaying ACKs beyond their useful lifetime.

## Memory safety and parser validation

- [x] **P0 `ACK-ONLY`: Bound multipart ACK construction.** `Mesh::createMultiAck()` can copy a full-size ACK after its one-byte wrapper and write beyond `Packet::payload`.
  - Corrective action: Reject lengths greater than `MAX_PACKET_PAYLOAD - 1` before allocation or copying, and add boundary tests for zero, canonical, maximum and oversized lengths.
  - Design constraint: Preserve the fixed packet payload limit and reject excess data rather than allocating dynamically on memory-constrained nodes.

- [ ] **P0 `ACK-ONLY`: Bound standalone ACK construction and enforce a canonical wire length.** `Mesh::createAck()` accepts caller-controlled lengths even though ACK consumers use only a four-byte identifier.
  - Corrective action: Define one versioned ACK structure and reject every non-canonical length at creation, receipt and relay boundaries.
  - Design constraint: Keep the canonical ACK compact enough for scarce airtime and provide an explicit compatibility path for existing wire formats.
  - Status: An implementation is preserved in the `fix-canonical-ack-length` stash, but it has not yet been completed, committed or integrated.

- [ ] **P0 `ALL-PACKETS`: Add structural bounds checks to decrypted PATH payload parsing.** The parser can advance beyond the decrypted data before reading the extra type and calculating the extra length.
  - Corrective action: Validate each encoded path component and the extra-type byte against the decrypted length before dereferencing or subtracting offsets.
  - Design constraint: Validation must be bounded by received length and require no unbounded buffering or external schema service.

- [ ] **P1 `ACK-ONLY`: Validate `multi_acks` at every configuration ingress.** Runtime CLI and companion-protocol writes can bypass the load-time restriction and values above 15 also overflow the four-bit multipart count field.
  - Corrective action: Centralise validation in the preference setter, constrain the value to the protocol-supported range, and reject invalid commands rather than silently wrapping them.
  - Design constraint: Independently operated nodes retain local configuration authority, but conforming firmware must enforce wire-format and local resource limits.

## Authentication, integrity and privacy

- [ ] **P0 `ACK-ONLY`: Replace unauthenticated 32-bit ACK bearer tokens.** Standalone ACKs can be guessed, replayed, injected or reused by any participant that can transmit on the mesh.
  - Corrective action: Carry a longer correlation identifier inside an authenticated peer message or add a cryptographic authenticator covering the complete ACK structure.
  - Design constraint: Authentication must remain peer-to-peer without a central certificate authority and must justify every added on-air byte and cryptographic cost.

- [ ] **P0 `ACK-ONLY`: Bind ACKs to both peers, the message domain and the session.** Current matching does not reliably identify the sender, recipient, message type or protocol session.
  - Corrective action: Include sender and recipient identities, message class, protocol version and a fresh per-message nonce in the authenticated ACK data.
  - Design constraint: Bind compact identity references or authenticated context without requiring relays to know global sessions or full endpoint identities.

- [ ] **P1 `ACK-ONLY`: Add explicit replay protection and expiry to ACK validation.** Delayed or captured ACKs can confirm unrelated or long-expired operations when identifiers are reused or collide.
  - Corrective action: Maintain a bounded active correlation set with creation and expiry times, and accept each authenticated ACK at most once.
  - Design constraint: Replay state must be finite; protection and at-most-once acceptance apply only within a documented local retention window.

- [ ] **P1 `ACK-ONLY`: Remove clear deterministic message fingerprints from ACK traffic.** Truncated hashes of timestamps, flags, text and public identity permit traffic correlation and low-entropy message guessing.
  - Corrective action: Use random correlation identifiers protected by authenticated encryption rather than deterministic clear-text message digests.
  - Design constraint: Random identifiers must be compact, locally generated and interoperable without central allocation or persistent global uniqueness.

- [ ] **P1 `ACK-ONLY`: Rate-limit requests that force ACK production.** Authenticated clients can generate excessive keepalive or message ACK traffic even when forged ACK injection is fixed.
  - Corrective action: Apply per-peer token buckets, minimum request intervals and aggregate node-wide limits before creating ACK packets.
  - Design constraint: Rate limits are enforced locally and must leave a small allowance for legitimate traffic after long outages or bursty reconnection.

## Routing and forwarding correctness

- [ ] **P0 `ACK-ONLY`: Process direct ACKs only at their final destination.** Every receiver currently invokes application ACK handling before confirming that it is the next hop or destination.
  - Corrective action: Separate forwarding from delivery and call the ACK consumer only after the direct path has been completely consumed by the intended destination.
  - Design constraint: Intermediate relays must remain stateless about application delivery and continue forwarding from compact path information alone.

- [ ] **P0 `ACK-ONLY`: Stop using packet-header corruption as an ACK-consumed marker.** `markDoNotRetransmit()` changes the protocol header to `0xFF`, which can corrupt a direct ACK before forwarding decisions finish.
  - Corrective action: Return an explicit consumed result or store local processing state outside the packet's on-air header.
  - Design constraint: Local state must not alter the interoperable wire packet and should add minimal per-packet memory overhead.

- [ ] **P1 `ACK-ONLY`: Mark relay-generated ACK packets as locally seen.** ACKs created by `routeDirectRecvAcks()` bypass the normal direct-send path that records their packet hashes.
  - Corrective action: Register each successfully queued replacement packet in the deduplication table using the same lifecycle as other locally originated packets.
  - Design constraint: Deduplication storage is finite, so relay-generated control packets must not consume unbounded history or displace useful data without policy.

- [ ] **P0 `ALL-PACKETS`: Mitigate direct-route branching caused by short next-hop hashes.** One-byte path hashes can match multiple neighbours, causing several nodes to forward the same direct packet simultaneously. Collisions cannot be eliminated while compact hashes remain available.
  - Corrective action: Default to longer path hashes where packet space permits and support locally configured or density-aware hash sizing for dense regions, while retaining compact hashes for long or sparse routes.
  - Design constraint: Compact hashes and occasional collisions are inherent; mitigation must preserve long routes and allow autonomous nodes to choose an appropriate local trade-off.

- [ ] **P1 `ACK-ONLY`: Prevent false ACK matches at intermediaries from suppressing the intended ACK.** A coincidental or injected token match can trigger local consumption or packet mutation before the ACK reaches its sender.
  - Corrective action: Combine final-destination-only delivery, peer-bound authenticated identifiers and non-destructive callback results so an intermediary cannot consume another node's ACK.
  - Design constraint: Relays cannot authenticate application ownership on behalf of endpoints and must not require shared global identity state.

- [ ] **P1 `ACK-ONLY`: Detect and repair stale or asymmetric reverse routes before repeated ACK failure.** A data packet can arrive successfully while every ACK follows an obsolete stored route.
  - Corrective action: Derive or validate a return route from authenticated delivery context, then trigger bounded route repair before retransmitting the original application message.
  - Design constraint: Stale and asymmetric routes are unavoidable; repair must be locally initiated, bounded in airtime and able to fall back when no route exists.

- [ ] **P1 `ACK-ONLY`: Prefer a route-learning response over standalone flood ACK fallback.** Some firmware floods ACKs without establishing a usable path, so later exchanges repeat the same network-wide flood pattern. A bounded flood fallback is still required when no usable return route can be established.
  - Corrective action: Prefer an authenticated PATH or RESPONSE that both confirms delivery and establishes the reciprocal route, falling back to a rate-limited flood when route learning is unavailable or fails.
  - Design constraint: Flooding cannot be eliminated because topology and routes are unmanaged; the fallback must remain interoperable and locally bounded.

- [x] **P1 `ACK-ONLY`: Apply a contention delay to every direct ACK relay.** The common `multi_acks == 0` path forwards ACKs with no ACK-specific retransmission delay.
  - Corrective action: Run all relayed ACKs through the adaptive contention scheduler, while allowing near-zero delay only when measured contention is genuinely low.
  - Design constraint: Scheduling can use only local radio observations and must keep ACK latency within a bounded usefulness window.
  - Completed by `c5f2067d`: all direct ACK relay paths now use the bounded airtime- and queue-pressure-aware scheduler.

- [x] **P1 `ACK-ONLY`: Separate the final ACK from the last multipart ACK in time.** Both packets are currently scheduled for the same timestamp and are transmitted as a back-to-back burst.
  - Corrective action: Give each copy an independently calculated transmission slot with a minimum separation and an overall usefulness deadline.
  - Design constraint: Limited redundancy remains valid on unreliable links, but every copy must consume a bounded local airtime budget and expire promptly.
  - Completed by `c5f2067d`: each copy receives fresh jitter and is separated by at least the preceding packet airtime plus a 50 ms guard.

- [ ] **P1 `ACK-ONLY`: Make `multi_acks` semantics consistent between origins and relays.** Endpoints treat the setting as a Boolean while relays treat it as a count.
  - Corrective action: Define the field's exact meaning in the protocol and use one shared implementation for origin and relay behaviour.
  - Design constraint: Preserve mixed-version interoperability and local operator choice while preventing a relay preference from recursively amplifying third-party traffic.

## Deduplication, replay and delivery semantics

- [ ] **P0 `ACK-ONLY`: Reject arbitrary ACK suffixes.** ACK consumers match four bytes while deduplication hashes the complete payload, so changing trailing bytes creates unlimited unique packets for one logical ACK.
  - Corrective action: Parse only a canonical ACK structure and calculate deduplication identity from its authenticated correlation identifier.
  - Design constraint: Canonicalization must retain a compact wire representation and an explicit transition strategy for legacy ACK variants.

- [ ] **P1 `ACK-ONLY`: Replace random ACK padding with an explicit transmission-attempt identity.** The additional random byte defeats logical duplicate suppression and increases airtime and deduplication-table churn, although unreliable links may still require distinguishable bounded transmission attempts.
  - Corrective action: Keep a canonical logical ACK identity and represent any required copy or attempt identity explicitly, so application deduplication can collapse copies while the scheduler performs bounded retransmission.
  - Design constraint: Unreliable links may require distinct attempts, but attempt metadata must remain compact and bounded separately from logical delivery identity.

- [ ] **P1 `ALL-PACKETS`: Do not irreversibly mark a packet as seen before forwarding is successfully queued.** Queue or allocation failure after `markSeen()` causes later redundant copies to be discarded even though no forward occurred.
  - Corrective action: Record a pending-forward state and commit the seen entry only after successful queue insertion, or allow a bounded retry after local forwarding failure.
  - Design constraint: Pending state and retries must be finite; duplicate suppression remains essential to prevent floods from multiplying under queue pressure.

- [ ] **P1 `ALL-PACKETS`: Protect useful data history from control-packet churn in the bounded deduplication table.** ACK bursts can evict recent data hashes and allow delayed data duplicates to propagate again. Some eviction is unavoidable on memory-constrained nodes.
  - Corrective action: Partition or weight finite deduplication capacity by packet class and use locally measured delay to choose bounded retention windows, without promising permanent duplicate suppression.
  - Design constraint: Eviction is unavoidable with finite RAM, and retention can rely only on local delay history rather than a network-wide maximum.

- [ ] **P1 `ALL-PACKETS`: Separate logical message identity from transmission-attempt identity.** Retries need distinct radio packet hashes, but changing the packet identity currently also makes the same application message appear new to receivers and deduplication logic.
  - Corrective action: Carry a stable authenticated message identifier plus a separate attempt identifier, and deduplicate application delivery on the stable identifier.
  - Design constraint: Both identifiers must fit the airtime budget, be generated without central coordination and have explicitly bounded lifetimes.

- [ ] **P1 `ACK-ONLY`: Provide at-most-once application delivery within a bounded retention window.** Loss of an ACK can cause a valid application message to be delivered and acted upon again when the sender retries; permanent at-most-once delivery is impossible with finite recipient storage.
  - Corrective action: Maintain a bounded recipient-side cache of authenticated message identifiers and return the stored ACK without re-running application side effects. Document the retention window and require longer-lived operations to be idempotent.
  - Design constraint: Permanent exactly-once behaviour is impossible with finite storage and ambiguous delivery; guarantees apply only inside the cache window.

- [ ] **P1 `ACK-ONLY`: Include the full retry attempt or nonce in ACK identity.** Extended attempts currently repeat the same logical ACK token every four attempts.
  - Corrective action: Use a non-wrapping per-message attempt field or a fresh cryptographic nonce that is included in both message authentication and ACK correlation.
  - Design constraint: Attempt identity must remain compact and locally unique for the bounded retry lifetime, not globally unique forever.

- [ ] **P1 `ACK-ONLY`: Prevent one recipient's ACK from confirming another recipient's message.** Identical content, timestamp and attempt values can produce the same expected token for different contacts.
  - Corrective action: Match ACK state by recipient identity plus a unique authenticated message identifier, never by token alone.
  - Design constraint: Peer binding must be achieved end-to-end without requiring relays or a central registry to track application contacts.

## ACK state management and matching

- [ ] **P1 `ACK-ONLY`: Add explicit active state instead of using token zero as a sentinel.** A four-zero ACK can match unused companion-table entries, while a legitimate zero token cannot be tracked.
  - Corrective action: Store an `active` flag separately from the identifier and validate all metadata before reporting confirmation.
  - Design constraint: The additional state must be explicit but compact because outstanding-operation tables are intentionally finite.

- [ ] **P1 `ACK-ONLY`: Manage the finite expected-ACK table without silent rollover.** Companion entries remain matchable until overwritten, and the ninth outstanding send replaces an earlier one without flow control. A fixed table capacity is necessary; silent replacement is not.
  - Corrective action: Reject or queue new acknowledged sends when the table is full, and remove entries through explicit success, failure or bounded expiry transitions.
  - Design constraint: Fixed capacity is necessary; backpressure must be local and must not imply that remote route capacity is known.

- [ ] **P1 `ACK-ONLY`: Replace the single global text-send timeout with per-message timers.** Multiple outstanding operations can overwrite or cancel each other's timeout state.
  - Corrective action: Attach the timer, peer, retry policy and callback to each active transmission record.
  - Design constraint: Per-message state must have a hard local bound and degrade to rejection or queueing when memory is exhausted.

- [x] **P1 `ACK-ONLY`: Do not start ACK timeout state for CLI data that has no ACK semantics.** `sendCommandData()` starts the generic text ACK timer even though receivers explicitly do not ACK CLI data.
  - Corrective action: Track CLI completion through its response or request identifier, or send it without ACK timeout state.
  - Design constraint: Completion semantics must remain compatible with peers that intentionally provide no ACK and must treat silence as indeterminate where no response is defined.
  - Completed on `fix-ack-retry-backoff`: command data still reports a conservative transport estimate but no longer creates or overwrites ACK timeout state.

- [ ] **P1 `ACK-ONLY`: Expire locally obsolete queued ACK copies.** Redundant or delayed ACKs remain in the outbound queue after their local usefulness deadline. A relay cannot know with certainty that a remote sender received another copy or moved on.
  - Corrective action: Associate queued ACKs with a local cancellation key and bounded deadline, then remove copies made obsolete by locally observed state or skip them after expiry.
  - Design constraint: A relay cannot know remote delivery state; cancellation may use only local queue state, authenticated local events and conservative expiry.

- [ ] **P1 `ACK-ONLY`: Keep room-server keepalive state separate from pending post-ACK state.** Keepalive processing currently clears the client's pending post acknowledgement and can break stop-and-wait delivery.
  - Corrective action: Use independent correlation and timeout records for keepalives, pushed posts and other request classes.
  - Design constraint: Independent records must remain bounded per client and tolerate clients disappearing without an orderly disconnect.

- [ ] **P1 `ACK-ONLY`: Clear all sensor alert ACK candidates when starting a new alert or contact.** Lower attempt slots can retain identifiers from an earlier operation and falsely confirm the current alert.
  - Corrective action: Reset the complete expected-ACK array and bind every populated entry to the current contact and alert identifier.
  - Design constraint: Sensor RAM and retry count are finite, so reset and binding must not introduce unbounded per-alert history.

- [ ] **P1 `ACK-ONLY`: Update sensor replay state after successful plain-message handling.** Accepted plain messages can be processed repeatedly because `last_timestamp` is not advanced on the acknowledged path.
  - Corrective action: Update replay state atomically with successful application processing and ACK creation, using a stronger message identifier than timestamp alone.
  - Design constraint: Replay history is locally bounded and must tolerate clock uncertainty, reboot and intermittent contact without relying on synchronized global time.

- [ ] **P1 `ACK-ONLY`: Preserve a bounded previous room-push identifier during retry transitions.** Clearing the old token immediately means a legitimately delayed ACK cannot resolve the previous attempt; finite memory prevents retaining every historical identifier.
  - Corrective action: Retain a small, expiring set of identifiers for the same logical message and collapse any matching success within that window into one delivery result.
  - Design constraint: Delayed ACKs outside the finite window remain indeterminate; historical identifiers cannot be retained indefinitely.

- [ ] **P1 `ACK-ONLY`: Fix simple secure chat ACK matching to return the matched contact.** The example prints successful receipt but returns `NULL`, so BaseChat keeps the timeout active and later reports failure.
  - Corrective action: Return the matched contact and add an integration test proving that the timeout is cancelled.
  - Design constraint: Preserve the example's lightweight single-operation model and do not infer delivery beyond the matched ACK.

- [ ] **P2 `ACK-ONLY`: Expose structured keepalive response data through the application interface.** The room server appends a fifth byte that the generic ACK callback cannot consume.
  - Corrective action: Move keepalive status into a versioned authenticated RESPONSE structure and reserve ACK for delivery confirmation only.
  - Design constraint: The response must stay compact, work peer-to-peer without central schema negotiation and provide a legacy transition path.

## Congestion, collision and airtime control

- [x] **P0 `ACK-ONLY`: Eliminate multi-ACK relay multiplication and relay-local redundancy.** When both the multipart and normal forms of a logical ACK reach a relay, their distinct deduplication identities can each regenerate `N` multipart packets plus one normal ACK. The relay's local `multi_acks` setting therefore permits up to `2 x (N + 1)` transmissions for that logical ACK at the relay and affects traffic originated by other nodes.
  - Corrective action: A conforming relay should forward at most one scheduled copy for each logical ACK event and enforce a small local copy budget. End-to-end redundancy metadata may guide cooperating nodes but cannot enforce a trusted route-wide cap across independently operated relays.
  - Design constraint: Nodes remain independently operated and may be non-conforming; protection must be enforceable locally without trusted route-wide coordination while retaining bounded redundancy.

- [ ] **P1 `ACK-ONLY`: Apply local admission control to standalone flood ACK forwarding.** Every syntactically acceptable unseen ACK can currently consume network-wide airtime regardless of whether any local operation expects it. The current standalone format has no trustworthy origin on which to base a per-origin limit.
  - Corrective action: Forward only canonical authenticated ACKs that satisfy local bounded policy, with per-origin limits when an authenticated origin exists and per-token plus aggregate node limits otherwise.
  - Design constraint: Admission is local, and origin-specific policy is impossible for legacy standalone ACKs that carry no authenticated origin.

- [x] **P2 `ACK-ONLY`: Implement locally contention-aware ACK scheduling.** ACK producers and relays previously used fixed or zero ACK-specific delays, allowing independent nodes to retain common transmission phases.
  - Corrective action: Use a conservative bounded scheduler based on packet airtime and local queue pressure, with fresh random jitter for every ACK transmission.
  - Design constraint: A node cannot observe hidden terminals or global density; the scheduler must use cheap local signals and conservative defaults.
  - Completed by `c5f2067d`: the jitter window scales from two to six packet airtimes with queue pressure and is clamped to 100-2000 ms.

- [x] **P2 `ACK-ONLY`: Break fixed ACK phase alignment.** Endpoint ACKs used a fixed 200 ms delay and multipart copies used fixed 300 ms spacing, allowing synchronised producers to retain common phases.
  - Corrective action: Add fresh bounded jitter to endpoint and relay ACKs and schedule redundant copies after the preceding packet's airtime and a minimum guard.
  - Design constraint: Jitter must be independently computable, bounded for high-latency links and require no shared clock or central schedule.
  - Completed by `c5f2067d`: ACK producers retain the 200 ms earliest-response guard but no longer transmit at a fixed offset, and multipart spacing is airtime-aware rather than fixed at 300 ms.

- [ ] **P2 `ACK-ONLY`: Validate and tune ACK scheduling in dense and hidden-terminal topologies.** The conservative scheduler breaks fixed phases, but its 100-2000 ms jitter bounds and queue-pressure multiplier have not been measured against realistic flood tails, asymmetric routes or hidden terminals.
  - Corrective action: Measure collision rate, queue delay, delivery latency and ACK overlap, then tune the existing bounds or add the smallest useful local signal such as recent channel occupancy, duplicate activity or CAD outcomes.
  - Design constraint: No endpoint can know when every flood branch has completed; only a bounded local estimate is possible.

- [x] **P1 `ACK-ONLY`: Add congestion-sensitive backoff to application ACK retries.** MAC-level CAD retries are already randomised in `Mesh`, but fixed application retry deadlines can preserve phase relationships after a collision.
  - Corrective action: Apply bounded exponential or adaptive backoff with fresh jitter to ACK-driven application retries, with a maximum retry age and attempt count. Handle fixed keepalive periods in the separate keepalive item below.
  - Design constraint: Retries remain necessary for unreliable links, and backoff must not grow beyond the application's useful delivery lifetime.
  - Completed on `fix-ack-retry-backoff`: BaseChat retries use fresh bounded exponential jitter, at most eight attempts and a five-minute operation lifetime.

- [ ] **P1 `ALL-PACKETS`: Bound starvation from strict queue priority while preserving control deadlines.** Direct ACK bursts use priority 0, as does ordinary direct data, while PATH, advert and flood traffic have lower priority. The strict-priority queue has no fairness or ageing, so ACK multiplication can amplify an existing all-packet starvation risk; completely equal scheduling would also be inappropriate for time-sensitive control traffic.
  - Corrective action: Introduce bounded per-class airtime, priority ageing, packet expiry and a protected allowance for route establishment while retaining high priority for ACKs that are still useful.
  - Design constraint: Fairness cannot mean equal service; time-sensitive control traffic needs priority while every class receives a bounded opportunity to progress.

- [ ] **P1 `ACK-ONLY`: Jitter and locally rate-limit keepalive schedules to prevent ACK implosion.** Connections established together can converge on identical keepalive phases and force the server to emit a burst of ACKs.
  - Corrective action: Randomise each next keepalive deadline after success and let each server enforce its own per-client plus aggregate limits without assuming network-wide coordination.
  - Design constraint: Schedules and limits are local because clients and servers are autonomous, intermittently connected and unsynchronized.

- [ ] **P2 `ACK-ONLY`: Make ACK redundancy conditional on locally estimated loss.** The static copy count cannot distinguish reliable from lossy routes, so it can waste airtime or worsen congestion; exact route loss is not globally observable.
  - Corrective action: After eliminating relay multiplication and adding delivery metrics, let each endpoint adjust redundancy from recent authenticated outcomes with conservative bounds, hysteresis and a static fallback when evidence is insufficient.
  - Design constraint: Loss estimates are local and noisy; bounded static redundancy must remain available after reboot or sparse observations.

- [ ] **P2 `ACK-ONLY`: Support ACK piggybacking and cumulative acknowledgement.** This optional protocol enhancement can reduce separate ACK packets when timely reverse-direction authenticated traffic exists.
  - Corrective action: Attach one or more correlation identifiers to the next authenticated peer packet, subject to a maximum acknowledgement delay.
  - Design constraint: Piggybacking is opportunistic because reverse traffic may never arrive; a bounded standalone ACK fallback remains necessary.

- [ ] **P2 `ACK-ONLY`: Define safe local defaults for acknowledged-operation in-flight limits.** Companion firmware permits several outstanding ACK-tracked operations, while room-server and sensor flows already use stop-and-wait or bounded application queues. Independently operated applications can legitimately choose different limits, and no node can enforce a shared route-wide congestion window.
  - Corrective action: Define bounded per-peer and locally estimated per-route defaults for ACK-tracked operations, preserve stricter application policies and reopen capacity on success, explicit rejection or controlled timeout.
  - Design constraint: Capacity is enforced by each endpoint independently; no node can know or impose a shared congestion window across an unmanaged route.

## Queueing, resource exhaustion and timing

- [ ] **P0 `ACK-ONLY`: Prevent multi-ACK settings from exhausting the packet pool.** One received ACK can allocate many future outbound packets from a pool shared with receive processing.
  - Corrective action: Allocate redundancy lazily at transmission time, enforce a small hard cap and refuse replication when the queue or pool crosses a safety threshold.
  - Design constraint: The shared pool is finite and redundancy is still useful on lossy links, so protection must degrade copy count before blocking essential receive work.

- [ ] **P1 `ALL-PACKETS`: Reserve receive capacity in the finite shared packet pool.** Future outbound packets can consume every object and cause newly received radio frames to be dropped. Rigidly separate pools may waste scarce memory when traffic is strongly asymmetric.
  - Corrective action: Prefer a small protected receive reserve or adaptive allocation threshold; use separate pools only on platforms where the memory and traffic model justify them.
  - Design constraint: RAM is scarce and traffic is asymmetric, so reserved capacity must be small and configurable rather than permanently stranding a large partition.

- [ ] **P1 `ALL-PACKETS`: Add transmission deadlines to the outbound queue.** Delayed packets can remain allocated and transmit after their application value has expired.
  - Corrective action: Store a latest-useful-send time on each queue entry and discard expired packets before CAD, airtime accounting or radio transmission.
  - Design constraint: Deadlines are application-local estimates; packets without meaningful expiry and essential control traffic need explicit conservative defaults.

- [ ] **P1 `ALL-PACKETS`: Start delivery timers from confirmed local transmission completion.** Current ACK timeouts begin when the original packet is queued, so queueing and duty-cycle delays consume the response window. Local completion does not reveal downstream route progress.
  - Corrective action: Have the dispatcher report local send completion and derive a conservative response deadline from that timestamp plus an estimated downstream delay budget.
  - Design constraint: Local radio completion is observable, but downstream forwarding is not; the resulting deadline remains an estimate rather than an end-to-end guarantee.
  - Status: Dispatcher completion and failure hooks now start BaseChat ACK timers from confirmed local transmission; other delivery timers still need migration before this all-packet item is complete.

- [x] **P1 `ACK-ONLY`: Suppress retries while the original transmission is locally queued and allow for estimated ACK transit.** A timeout can currently create a second application transmission while the first packet is still queued or its ACK may still be traversing the route. Remote in-flight state cannot be known exactly.
  - Corrective action: Track local transmission state explicitly and apply a bounded late-ACK grace period derived from recent route observations before creating a new attempt.
  - Design constraint: Remote in-flight state is unknowable and silence is ambiguous, so suppression must end after a bounded grace period and permit necessary retries.
  - Completed on `fix-ack-retry-backoff`: identical BaseChat operations reuse the active ACK token while locally queued or within the ACK window, without allocating another packet.

- [ ] **P1 `ALL-PACKETS`: Include locally known and estimated transport delays in timeout calculation.** Current timeouts do not account for local queue depth, CAD deferral, receive delay, duty-cycle limits, route length or scheduled redundancy. Future delays elsewhere on an unmanaged route cannot be known exactly.
  - Corrective action: Calculate an adaptive deadline from local scheduler state, recent route latency and a bounded safety margin, with conservative defaults when history is absent.
  - Design constraint: Route conditions change without notice; calculations may use only local history and must cap both waiting time and retry age.
  - Status: BaseChat estimates now include retry scheduling, bounded queue pressure, a CAD allowance and encoded direct-path airtime. Duty-cycle state, receive delay and route history remain outstanding.

- [ ] **P1 `ACK-ONLY`: Keep local receive-side delay within the advertised ACK budget.** Flood ACK or PATH packets can remain in a node's delayed inbound queue longer than application retry intervals, while an end-to-end delay guarantee is impossible across independently configured relays.
  - Corrective action: Bound local delay for time-critical authenticated control responses and include a conservative per-hop delay allowance in sender timeout calculation.
  - Design constraint: Independently configured relays cannot promise a common end-to-end latency, so each node can enforce only its own delay contribution.

- [ ] **P1 `ACK-ONLY`: Bound avoidable control traffic created by flood delivery.** One message can produce the original flood, a PATH carrying an ACK, a reciprocal PATH and later route-repair traffic. Topology changes and missing routes mean no fixed packet minimum is achievable in every case.
  - Corrective action: Combine route establishment and confirmation where possible, apply local packet and airtime budgets, and suppress redundant repair while equivalent local work is pending.
  - Design constraint: Flooding and route repair remain necessary when topology is unknown; only avoidable local duplication can be bounded reliably.

## Protocol semantics and interoperability

- [ ] **P1 `ACK-ONLY`: Define ACK expectations for every payload and subtype.** Sender and receiver implementations disagree about whether plain, guest, CLI, keepalive and application-specific messages should be acknowledged.
  - Corrective action: Publish a machine-readable policy stating `required`, `optional`, `response-completes` or `prohibited` for every message subtype and enforce it at send time.
  - Design constraint: Policy must be versioned and backward compatible because independently upgraded firmware variants will coexist for long periods.

- [ ] **P1 `ACK-ONLY`: Distinguish explicit rejection and non-acknowledged delivery from indeterminate timeout.** The sender currently interprets every missing ACK as a transmission failure and may retry traffic that the receiver deliberately will not acknowledge. Silence cannot distinguish loss, delay, remote processing or a lost response.
  - Corrective action: Use authenticated status responses for accepted, rejected and unsupported operations, disable retries when the protocol defines no ACK, and report timeout without a response as indeterminate rather than proven failure.
  - Design constraint: No protocol can infer remote outcome from silence on a lossy asynchronous mesh; certainty is limited to authenticated responses that actually arrive.

- [ ] **P2 `ACK-ONLY`: Correct ACK terminology and document the actual wire format.** Documentation calls the identifier a CRC and specifies four bytes while implementations emit several lengths and extensions.
  - Corrective action: Document one versioned structure, its cryptographic construction, exact length, matching rules, routing behaviour and extension mechanism.
  - Design constraint: Documentation must preserve compact wire encoding and define coexistence with legacy nodes rather than require coordinated network-wide upgrades.

- [ ] **P2 `ACK-ONLY`: Prefer RESPONSE over ACK for request-response operations.** Keepalive and CLI-style exchanges need structured result data rather than an opaque delivery token.
  - Corrective action: Treat a correlated authenticated RESPONSE as completion and reserve ACK for messages that otherwise have no response body.
  - Design constraint: RESPONSE adds airtime and may be unsupported by legacy peers, so compact ACK-only completion remains valid where no result body is needed.

## Application-specific flow-control defects

- [ ] **P1 `ACK-ONLY`: Prevent room guest posts from entering an ACK retry loop.** Generic senders can expect an ACK while the room server intentionally does not acknowledge guest posts.
  - Corrective action: Advertise the room's delivery policy before sending or return an authenticated acceptance status that terminates sender retry logic.
  - Design constraint: Rooms are autonomously operated and older clients may not understand advertised policy; unsupported or silent outcomes must remain bounded and indeterminate.

- [ ] **P1 `ACK-ONLY`: Ensure sensor handlers declare idempotence and delivery semantics.** Conditional ACK generation means application failure, deliberate rejection, request loss and ACK loss are indistinguishable when no response arrives.
  - Corrective action: Make retryable operations idempotent where practical, return a structured result from the handler, and map received results to confirmed success or explicit rejection while treating silence as indeterminate.
  - Design constraint: Sensors have tight memory and power budgets, and no response can distinguish request loss from response loss; semantics must remain locally bounded.

- [ ] **P1 `ACK-ONLY`: Separate room-post synchronisation progress from ACK transport state.** Late, cleared or mismatched ACKs can leave `sync_since` unchanged and cause the same post to be pushed again.
  - Corrective action: Commit synchronisation progress against the authenticated message identifier in an idempotent state transition.
  - Design constraint: Synchronisation state is finite and local, must survive intermittent contact, and cannot depend on continuous server-client coordination.

## Observability, verification and testing

- [ ] **P1 `ALL-PACKETS`: Instrument ACK-driven congestion and collision behaviour.** Existing statistics do not fully expose logical ACKs, generated copies, relay fan-out, late arrivals, queue delay or ACK-triggered retries.
  - Corrective action: Add counters and histograms for ACK production, forwarding, duplicate suppression, pool failures, CAD deferrals, expiry, route length, copies per hop and retry causality.
  - Design constraint: Telemetry must be bounded, low-overhead and local; nodes cannot provide a complete or centrally trusted view of mesh-wide behaviour.

- [ ] **P2 `ALL-PACKETS`: Record packet-class queue and airtime consumption.** Without per-class accounting it is difficult to identify ACK starvation, control-plane bursts or data displacement.
  - Corrective action: Track queued time, transmitted airtime, drops and forced sends by payload type, priority and route mode.
  - Design constraint: Per-class accounting must use fixed-size counters and must not materially consume the airtime, RAM or energy it is intended to protect.

- [ ] **P1 `ALL-PACKETS`: Add property tests and fuzzing for packet construction and parsing.** Boundary lengths, malformed PATH data, unusual multipart counts and parser underflow paths need systematic coverage.
  - Corrective action: Fuzz constructors and receive dispatch with sanitizers enabled, and assert that no invalid packet reaches application callbacks or the radio queue.
  - Design constraint: Host-side tests may use extensive resources, but production validation must remain deterministic, bounded and suitable for small embedded targets.

- [ ] **P1 `ACK-ONLY`: Add end-to-end ACK state-machine tests.** Current examples contain divergent handling, stale state and timeout cancellation defects.
  - Corrective action: Test direct, flood, PATH-embedded, duplicate, delayed, replayed, malformed, lost and retried ACK scenarios across every firmware variant.
  - Design constraint: Tests must cover mixed versions, finite retention windows and indeterminate timeouts rather than assume reliable links or coordinated upgrades.

- [ ] **P1 `ALL-PACKETS`: Add dense-topology and hidden-terminal simulations.** Path-hash collisions, synchronised timers and adaptive contention cannot be assessed adequately with single-link tests, while simulation cannot fully reproduce volunteer-operated topology and radio conditions.
  - Corrective action: Simulate linear, clustered and mixed topologies with controlled loss, neighbour counts and channel occupancy, then use the results to choose conservative local bounds rather than claim universal airtime or latency guarantees.
  - Design constraint: Simulation cannot reproduce every volunteer topology, radio environment or non-conforming node; results guide local defaults rather than establish global guarantees.
