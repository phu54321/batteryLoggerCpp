<script setup lang="ts">
import { computed, ref } from 'vue'

type BatteryLogRow = {
  time: string
  timestamp: number
  plugged: boolean
  percent: number
  machineId: string
}

type BatterySegment = {
  id: string
  machineId: string
  plugged: boolean
  startTime: string
  endTime: string
  startTimestamp: number
  endTimestamp: number
  startPercent: number
  endPercent: number
}

const embeddedCsvSlot = String.raw`<<<<<>>>>>`
const embeddedCsv = embeddedCsvSlot.slice(5, embeddedCsvSlot.length - 5).trim()

const csvText = ref(embeddedCsv)
const loadError = ref('')

const rows = computed(() => parseBatteryLog(csvText.value))
const segments = computed(() =>
  createBatterySegments(rows.value).filter((segment) => segment.startTimestamp !== segment.endTimestamp),
)
const machineIds = computed(() => Array.from(new Set(rows.value.map((row) => row.machineId))))
const chargingSegments = computed(() => segments.value.filter((segment) => segment.plugged))
const dischargingSegments = computed(() => segments.value.filter((segment) => !segment.plugged))

const dischargeSleepThresholdMs = 5 * 60 * 1000

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
  const elapsedHours = (segment.endTimestamp - segment.startTimestamp) / 3600000

  if (elapsedHours <= 0) {
    return '-'
  }

  const percentChange = segment.endPercent - segment.startPercent
  const signedRate = percentChange / elapsedHours

  return `${signedRate > 0 ? '+' : ''}${signedRate.toFixed(2)}%/hr`
}

async function loadDevelopmentCsv() {
  const privateCsvPath = './assets/testdata/batteryLog.local.csv?raw'
  const sampleCsvPath = './assets/testdata/batteryLog.csv?raw'

  try {
    const privateCsv = await import(/* @vite-ignore */ privateCsvPath)
    csvText.value = privateCsv.default
    return
  } catch {
    // The private file is intentionally optional and gitignored.
  }

  try {
    const sampleCsv = await import(/* @vite-ignore */ sampleCsvPath)
    csvText.value = sampleCsv.default
  } catch (error) {
    loadError.value = error instanceof Error ? error.message : String(error)
  }
}

if (import.meta.env.DEV) {
  loadDevelopmentCsv()
}
</script>

<template>
  <main class="app-shell">
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

    <section v-if="loadError" class="notice notice-error">
      {{ loadError }}
    </section>

    <section v-else-if="rows.length === 0" class="notice">
      No battery log data found. In development, add
      <code>src/assets/testdata/batteryLog.local.csv</code>, or let the C++ app inject CSV into
      <code>&lt;&lt;&lt;&lt;&lt;&gt;&gt;&gt;&gt;&gt;</code>.
    </section>

    <section v-else class="table-wrap" aria-label="Battery segment table">
      <div class="table-header">
        <h2>Segments</h2>
        <p>{{ rows.length }} samples across {{ machineIds.length }} machine{{ machineIds.length === 1 ? '' : 's' }}</p>
      </div>
      <table>
        <thead>
          <tr>
            <th>Machine</th>
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
          <tr v-for="segment in segments" :key="segment.id">
            <td>{{ segment.machineId }}</td>
            <td>
              <span class="mode-pill" :class="segment.plugged ? 'mode-charging' : 'mode-discharging'">
                {{ segment.plugged ? 'Charging' : 'Discharging' }}
              </span>
            </td>
            <td>{{ segment.startTime }}</td>
            <td>{{ segment.endTime }}</td>
            <td>{{ formatDuration(segment) }}</td>
            <td>{{ segment.startPercent }}%</td>
            <td>{{ segment.endPercent }}%</td>
            <td>{{ formatPercentChange(segment) }}</td>
            <td>{{ formatRate(segment) }}</td>
          </tr>
        </tbody>
      </table>
    </section>
  </main>
</template>

<style scoped>
.app-shell {
  min-height: 100vh;
  padding: 32px;
  background: #f6f7f9;
  color: #1c2530;
  font-family:
    Inter, ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
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
.table-wrap {
  max-width: 1120px;
  margin: 0 auto;
  border: 1px solid #d9dee7;
  border-radius: 8px;
  background: #ffffff;
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

.mode-pill {
  display: inline-flex;
  align-items: center;
  min-width: 92px;
  justify-content: center;
  padding: 4px 8px;
  border-radius: 999px;
  font-size: 12px;
  font-weight: 700;
}

.mode-charging {
  background: #e7f6ec;
  color: #1c6b3a;
}

.mode-discharging {
  background: #fff1dc;
  color: #875015;
}

@media (max-width: 760px) {
  .app-shell {
    padding: 20px;
  }

  .app-header {
    align-items: stretch;
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
