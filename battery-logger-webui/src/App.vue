<script setup lang="ts">
import { computed, ref } from 'vue'

type BatteryLogRow = {
  time: string
  plugged: boolean
  percent: number
  machineId: string
}

const embeddedCsvSlot = String.raw`<<<<<>>>>>`
const embeddedCsv = embeddedCsvSlot.slice(5, embeddedCsvSlot.length - 5).trim()

const csvText = ref(embeddedCsv)
const loadError = ref('')

const rows = computed(() => parseBatteryLog(csvText.value))
const machineIds = computed(() => Array.from(new Set(rows.value.map((row) => row.machineId))))
const unpluggedRows = computed(() => rows.value.filter((row) => !row.plugged))
const visibleRows = computed(() => rows.value.slice(0, 200))

function parseBatteryLog(csv: string): BatteryLogRow[] {
  return csv
    .replaceAll('\0', '')
    .split(/\r?\n/)
    .map((line) => line.trim())
    .filter(Boolean)
    .filter((line) => !line.startsWith('time,'))
    .map((line) => {
      const [time = '', plugged = '', percent = '', machineId = ''] = line.split(',')

      return {
        time,
        plugged: plugged === 'True',
        percent: Number.parseInt(percent, 10),
        machineId,
      }
    })
    .filter((row) => row.time && Number.isFinite(row.percent))
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
          <span>{{ rows.length }}</span>
          <small>Rows</small>
        </div>
        <div>
          <span>{{ machineIds.length }}</span>
          <small>Machines</small>
        </div>
        <div>
          <span>{{ unpluggedRows.length }}</span>
          <small>Unplugged</small>
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

    <section v-else class="table-wrap" aria-label="Battery log table">
      <table>
        <thead>
          <tr>
            <th>Time</th>
            <th>Machine</th>
            <th>Power</th>
            <th>Percent</th>
          </tr>
        </thead>
        <tbody>
          <tr v-for="row in visibleRows" :key="`${row.machineId}-${row.time}`">
            <td>{{ row.time }}</td>
            <td>{{ row.machineId }}</td>
            <td>{{ row.plugged ? 'Plugged' : 'Battery' }}</td>
            <td>{{ row.percent }}%</td>
          </tr>
        </tbody>
      </table>
      <p v-if="rows.length > visibleRows.length" class="table-note">
        Showing first {{ visibleRows.length }} of {{ rows.length }} rows.
      </p>
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

.table-note {
  margin: 0;
  padding: 12px 14px;
  border-top: 1px solid #e7eaf0;
  color: #687383;
  font-size: 13px;
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

  .table-wrap {
    overflow-x: auto;
  }
}
</style>
