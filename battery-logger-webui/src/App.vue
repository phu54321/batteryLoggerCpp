<script setup lang="ts">
import { computed, ref, watch } from 'vue'
import BatteryGraph from '@/components/BatteryGraph.vue'
import type { BatteryLogRow, BatterySegment } from '@/types/battery'

const embeddedCsvSlot = String.raw`<<<<<>>>>>`
const embeddedCsv = embeddedCsvSlot.slice(5, embeddedCsvSlot.length - 5).trim()

const csvText = ref(embeddedCsv)
const loadError = ref('')
const loadedFileName = ref('')
const isDraggingFile = ref(false)
let dragDepth = 0

const rows = computed(() => parseBatteryLog(csvText.value))
const machineIds = computed(() => Array.from(new Set(rows.value.map((row) => row.machineId))))
const latestMachineId = computed(() => rows.value.at(-1)?.machineId ?? '')
const machineOptions = computed(() =>
  machineIds.value.map((machineId) => ({
    id: machineId,
    sampleCount: rows.value.filter((row) => row.machineId === machineId).length,
  })),
)
const selectedMachineId = ref('')
const machineRows = computed(() =>
  selectedMachineId.value === ''
    ? rows.value
    : rows.value.filter((row) => row.machineId === selectedMachineId.value),
)
const segments = computed(() =>
  createBatterySegments(machineRows.value).filter(
    (segment) => segment.startTimestamp !== segment.endTimestamp,
  ),
)
const chargingSegments = computed(() => segments.value.filter((segment) => segment.plugged))
const dischargingSegments = computed(() => segments.value.filter((segment) => !segment.plugged))

const dischargeSleepThresholdMs = 5 * 60 * 1000
const activeSegmentId = ref('')
const segmentRowElements = new Map<string, HTMLElement>()

function parseBatteryLog(csv: string): BatteryLogRow[] {
  return csv
    .replaceAll('\0', '')
    .split(/\r?\n/)
    .map((line) => line.trim())
    .filter(Boolean)
    .filter((line) => !line.startsWith('time,'))
    .map((line) => {
      const [time = '', plugged = '', percent = '', machineId = ''] = line.split(',')
      const timestamp = Date.parse(time)

      return {
        time,
        timestamp,
        plugged: plugged === 'True',
        percent: Number.parseInt(percent, 10),
        machineId,
      }
    })
    .filter((row) => row.time && Number.isFinite(row.timestamp) && Number.isFinite(row.percent))
}

function createBatterySegments(logRows: BatteryLogRow[]): BatterySegment[] {
  const batterySegments: BatterySegment[] = []
  let currentSegment: BatterySegment | null = null

  for (const row of logRows) {
    if (currentSegment === null) {
      currentSegment = createSegment(row, batterySegments.length)
      continue
    }

    const isSameMachine = currentSegment.machineId === row.machineId
    const isSamePowerState = currentSegment.plugged === row.plugged
    const timeSinceLastSample = row.timestamp - currentSegment.endTimestamp
    const shouldSplitForSleep =
      !currentSegment.plugged && timeSinceLastSample > dischargeSleepThresholdMs

    if (!isSameMachine || !isSamePowerState || shouldSplitForSleep) {
      batterySegments.push(currentSegment)
      currentSegment = createSegment(row, batterySegments.length)
      continue
    }

    currentSegment = {
      ...currentSegment,
      endTime: row.time,
      endTimestamp: row.timestamp,
      endPercent: row.percent,
    }
  }

  if (currentSegment !== null) {
    batterySegments.push(currentSegment)
  }

  return batterySegments
}

function createSegment(row: BatteryLogRow, index: number): BatterySegment {
  return {
    id: `${row.machineId}-${row.time}-${index}`,
    machineId: row.machineId,
    plugged: row.plugged,
    startTime: row.time,
    endTime: row.time,
    startTimestamp: row.timestamp,
    endTimestamp: row.timestamp,
    startPercent: row.percent,
    endPercent: row.percent,
  }
}

function formatDuration(segment: BatterySegment): string {
  const totalMinutes = Math.max(0, Math.round((segment.endTimestamp - segment.startTimestamp) / 60000))
  const hours = Math.floor(totalMinutes / 60)
  const minutes = totalMinutes % 60

  if (hours === 0) {
    return `${minutes}m`
  }

  return `${hours}h ${minutes}m`
}

function formatPercentChange(segment: BatterySegment): string {
  const percentChange = segment.endPercent - segment.startPercent
  const sign = percentChange > 0 ? '+' : ''

  return `${sign}${percentChange}%`
}

function formatRate(segment: BatterySegment): string {
  const elapsedMs = segment.endTimestamp - segment.startTimestamp

  if (elapsedMs < 10 * 60 * 1000) {
    return '-'
  }

  const elapsedHours = elapsedMs / 3600000
  const percentChange = segment.endPercent - segment.startPercent
  const signedRate = percentChange / elapsedHours

  return `${signedRate > 0 ? '+' : ''}${signedRate.toFixed(2)}%/hr`
}

function rateTone(segment: BatterySegment): string {
  if (segment.endTimestamp - segment.startTimestamp < 10 * 60 * 1000) {
    return ''
  }

  const percentChange = segment.endPercent - segment.startPercent

  if (percentChange > 0) {
    return 'rate-positive'
  }

  if (percentChange < 0) {
    return 'rate-negative'
  }

  return ''
}

function setSegmentRowRef(segmentId: string, element: unknown) {
  if (element instanceof HTMLElement) {
    segmentRowElements.set(segmentId, element)
    return
  }

  segmentRowElements.delete(segmentId)
}

function scrollSegmentRowIntoView(segmentId: string) {
  segmentRowElements.get(segmentId)?.scrollIntoView({
    behavior: 'smooth',
    block: 'center',
  })
}

function activateSegment(segmentId: string, shouldScrollTable = false) {
  activeSegmentId.value = segmentId

  if (shouldScrollTable) {
    scrollSegmentRowIntoView(segmentId)
  }
}

function resetDragState() {
  dragDepth = 0
  isDraggingFile.value = false
}

function handleDragEnter(event: DragEvent) {
  event.preventDefault()
  dragDepth += 1
  isDraggingFile.value = true
}

function handleDragOver(event: DragEvent) {
  event.preventDefault()

  if (event.dataTransfer !== null) {
    event.dataTransfer.dropEffect = 'copy'
  }
}

function handleDragLeave(event: DragEvent) {
  event.preventDefault()
  dragDepth = Math.max(0, dragDepth - 1)

  if (dragDepth === 0) {
    isDraggingFile.value = false
  }
}

async function handleDrop(event: DragEvent) {
  event.preventDefault()
  resetDragState()

  const file = event.dataTransfer?.files.item(0)

  if (file === undefined || file === null) {
    return
  }

  if (!file.name.toLowerCase().endsWith('.csv')) {
    loadError.value = 'Drop a CSV file exported by Battery Logger.'
    return
  }

  try {
    csvText.value = await file.text()
    loadedFileName.value = file.name
    loadError.value = ''
  } catch (error) {
    loadError.value = error instanceof Error ? error.message : String(error)
  }
}

async function loadDevelopmentCsv() {
  const privateCsvPath = './assets/testdata/batteryLog.local.csv?raw'
  const sampleCsvPath = './assets/testdata/batteryLog.csv?raw'

  try {
    const privateCsv = await import(/* @vite-ignore */ privateCsvPath)
    csvText.value = privateCsv.default
    loadedFileName.value = 'batteryLog.local.csv'
    return
  } catch {
    // The private file is intentionally optional and gitignored.
  }

  try {
    const sampleCsv = await import(/* @vite-ignore */ sampleCsvPath)
    csvText.value = sampleCsv.default
    loadedFileName.value = 'sample batteryLog.csv'
  } catch (error) {
    loadError.value = error instanceof Error ? error.message : String(error)
  }
}

if (import.meta.env.DEV) {
  loadDevelopmentCsv()
}

watch(
  machineIds,
  (ids) => {
    if (ids.length === 0) {
      selectedMachineId.value = ''
      return
    }

    if (!selectedMachineId.value || !ids.includes(selectedMachineId.value)) {
      selectedMachineId.value = latestMachineId.value || ids[0] || ''
    }
  },
  { immediate: true },
)

watch(segments, () => {
  activeSegmentId.value = segments.value[0]?.id ?? ''
})
</script>

<template>
  <main
    class="app-shell"
    :class="{ 'app-shell-dragging': isDraggingFile }"
    @dragenter="handleDragEnter"
    @dragover="handleDragOver"
    @dragleave="handleDragLeave"
    @drop="handleDrop"
  >
    <div v-if="isDraggingFile" class="drop-overlay">
      <div>Drop batteryLog.csv to load it</div>
    </div>

    <header class="app-header">
      <div>
        <p class="eyebrow">Battery Logger</p>
        <h1>Battery log viewer</h1>
      </div>
      <div class="summary-strip">
        <div>
          <span>{{ segments.length }}</span>
          <small>Segments</small>
        </div>
        <div>
          <span>{{ chargingSegments.length }}</span>
          <small>Charging</small>
        </div>
        <div>
          <span>{{ dischargingSegments.length }}</span>
          <small>Discharging</small>
        </div>
      </div>
    </header>

    <section class="drop-hint" aria-label="CSV drag and drop">
      <span>Drop a battery log CSV anywhere on this page</span>
      <strong v-if="loadedFileName">{{ loadedFileName }}</strong>
    </section>

    <section v-if="loadError" class="notice notice-error">
      {{ loadError }}
    </section>

    <section v-else-if="rows.length === 0" class="notice">
      No battery log data found. In development, add
      <code>src/assets/testdata/batteryLog.local.csv</code>, or let the C++ app inject CSV into
      <code>&lt;&lt;&lt;&lt;&lt;&gt;&gt;&gt;&gt;&gt;</code>.
    </section>

    <template v-else>
      <BatteryGraph
        :rows="machineRows"
        :segments="segments"
        :active-segment-id="activeSegmentId"
        @activate-segment="(segmentId) => activateSegment(segmentId, true)"
      />

      <section class="table-wrap" aria-label="Battery segment table">
      <div class="table-header">
        <div>
          <h2>Segments</h2>
          <p>
            {{ machineRows.length }} samples for machine
            <strong>{{ selectedMachineId }}</strong>
          </p>
        </div>
        <label class="machine-picker">
          <span>Machine</span>
          <select v-model="selectedMachineId">
            <option v-for="machine in machineOptions" :key="machine.id" :value="machine.id">
              {{ machine.id }} ({{ machine.sampleCount }} samples)
            </option>
          </select>
        </label>
      </div>
      <table>
        <thead>
          <tr>
            <th>Mode</th>
            <th>Start</th>
            <th>End</th>
            <th>Duration</th>
            <th>Start %</th>
            <th>End %</th>
            <th>Change</th>
            <th>Rate</th>
          </tr>
        </thead>
        <tbody>
          <tr
            v-for="segment in segments"
            :key="segment.id"
            :ref="(element) => setSegmentRowRef(segment.id, element)"
            :class="[
              segment.plugged ? 'segment-charging' : 'segment-discharging',
              { 'segment-active': segment.id === activeSegmentId },
            ]"
            @click="activateSegment(segment.id)"
          >
            <td>
              <span class="mode-pill">
                {{ segment.plugged ? 'Charging' : 'Discharging' }}
              </span>
            </td>
            <td>{{ segment.startTime }}</td>
            <td>{{ segment.endTime }}</td>
            <td>{{ formatDuration(segment) }}</td>
            <td>{{ segment.startPercent }}%</td>
            <td>{{ segment.endPercent }}%</td>
            <td>{{ formatPercentChange(segment) }}</td>
            <td>
              <span class="rate-value" :class="rateTone(segment)">
                {{ formatRate(segment) }}
              </span>
            </td>
          </tr>
        </tbody>
      </table>
      </section>
    </template>
  </main>
</template>

<style scoped>
.app-shell {
  position: relative;
  min-height: 100vh;
  padding: 32px;
  background: #f6f7f9;
  color: #1c2530;
  font-family:
    Inter, ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
}

.app-shell-dragging {
  background: #eef6f2;
}

.drop-overlay {
  position: fixed;
  inset: 16px;
  z-index: 30;
  display: grid;
  place-items: center;
  border: 2px dashed #15773b;
  border-radius: 12px;
  background: rgba(243, 251, 245, 0.82);
  color: #145c31;
  font-size: 24px;
  font-weight: 800;
  pointer-events: none;
}

.app-header {
  display: flex;
  align-items: end;
  justify-content: space-between;
  gap: 24px;
  max-width: 1120px;
  margin: 0 auto 24px;
}

.eyebrow {
  margin: 0 0 8px;
  color: #5b6472;
  font-size: 13px;
  font-weight: 700;
  letter-spacing: 0;
  text-transform: uppercase;
}

h1 {
  margin: 0;
  font-size: 32px;
  line-height: 1.15;
}

.summary-strip {
  display: flex;
  gap: 12px;
}

.summary-strip div {
  min-width: 104px;
  padding: 12px 16px;
  border: 1px solid #d9dee7;
  border-radius: 8px;
  background: #ffffff;
}

.summary-strip span,
.summary-strip small {
  display: block;
}

.summary-strip span {
  font-size: 24px;
  font-weight: 750;
}

.summary-strip small {
  color: #687383;
  font-size: 12px;
}

.notice,
.table-wrap,
.drop-hint {
  max-width: 1120px;
  margin: 0 auto;
  border: 1px solid #d9dee7;
  border-radius: 8px;
  background: #ffffff;
}

.drop-hint {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 16px;
  margin-bottom: 18px;
  padding: 12px 16px;
  color: #687383;
  font-size: 13px;
}

.drop-hint strong {
  color: #1c2530;
  font-weight: 750;
}

.notice {
  padding: 20px;
  color: #4d5968;
}

.notice-error {
  border-color: #f0b8b8;
  color: #8d2525;
}

.table-wrap {
  overflow: hidden;
}

.table-header {
  display: flex;
  align-items: baseline;
  justify-content: space-between;
  gap: 16px;
  padding: 18px 20px;
  border-bottom: 1px solid #e7eaf0;
}

.table-header h2,
.table-header p {
  margin: 0;
}

.table-header h2 {
  font-size: 18px;
}

.table-header p {
  color: #687383;
  font-size: 13px;
}

.machine-picker {
  display: grid;
  gap: 6px;
  min-width: 180px;
}

.machine-picker span {
  color: #687383;
  font-size: 12px;
  font-weight: 700;
  text-transform: uppercase;
}

.machine-picker select {
  width: 100%;
  padding: 8px 32px 8px 10px;
  border: 1px solid #cbd3df;
  border-radius: 8px;
  background: #ffffff;
  color: #1c2530;
  font: inherit;
}

table {
  width: 100%;
  border-collapse: collapse;
  font-size: 14px;
}

th,
td {
  padding: 10px 14px;
  border-bottom: 1px solid #e7eaf0;
  text-align: left;
  white-space: nowrap;
}

th {
  background: #eef2f6;
  color: #3f4a58;
  font-size: 12px;
  text-transform: uppercase;
}

tr:last-child td {
  border-bottom: 0;
}

.segment-charging {
  background: #f3fbf5;
}

.segment-discharging {
  background: #fff8ed;
}

.segment-active {
  outline: 2px solid #24364b;
  outline-offset: -2px;
}

.mode-pill {
  display: inline-flex;
  align-items: center;
  min-width: 92px;
  justify-content: center;
  padding: 4px 8px;
  border-radius: 999px;
  border: 1px solid #cbd3df;
  background: rgba(255, 255, 255, 0.72);
  color: #3f4a58;
  font-size: 12px;
  font-weight: 700;
}

.rate-value {
  font-weight: 750;
}

.rate-positive {
  color: #15773b;
}

.rate-negative {
  color: #b42318;
}

@media (max-width: 760px) {
  .app-shell {
    padding: 20px;
  }

  .app-header {
    align-items: stretch;
    flex-direction: column;
  }

  .drop-hint {
    align-items: flex-start;
    flex-direction: column;
  }

  .summary-strip {
    display: grid;
    grid-template-columns: repeat(3, minmax(0, 1fr));
  }

  .summary-strip div {
    min-width: 0;
  }

  .table-header {
    align-items: flex-start;
    flex-direction: column;
  }

  .table-wrap {
    overflow-x: auto;
  }
}
</style>
