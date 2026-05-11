<script setup lang="ts">
import { computed, nextTick, onBeforeUnmount, onMounted, ref, watch } from 'vue'
import type { BatteryLogRow, BatterySegment } from '@/types/battery'

const props = defineProps<{
  rows: BatteryLogRow[]
  segments: BatterySegment[]
  activeSegmentId: string
}>()

const emit = defineEmits<{
  activateSegment: [segmentId: string]
}>()

const height = 260
const axisWidth = 48
const rightPadding = 24
const margin = {
  top: 20,
  bottom: 32,
}
const plotHeight = height - margin.top - margin.bottom
const dayMs = 24 * 60 * 60 * 1000
const minVisibleDays = 1
const maxVisibleDays = 7
const visibleDays = ref(2)
const graphScroller = ref<HTMLElement | null>(null)
const viewportWidth = ref(928)
let resizeObserver: ResizeObserver | null = null

const minTimestamp = computed(() =>
  props.rows.length === 0 ? 0 : Math.min(...props.rows.map((row) => row.timestamp)),
)
const maxTimestamp = computed(() =>
  props.rows.length === 0 ? 1 : Math.max(...props.rows.map((row) => row.timestamp)),
)
const timestampRange = computed(() => Math.max(1, maxTimestamp.value - minTimestamp.value))
const plotWidth = computed(() =>
  Math.max(viewportWidth.value, (timestampRange.value / (visibleDays.value * dayMs)) * viewportWidth.value),
)
const svgWidth = computed(() => plotWidth.value + rightPadding)

const linePoints = computed(() =>
  props.rows
    .map((row) => `${xForTimestamp(row.timestamp).toFixed(2)},${yForPercent(row.percent).toFixed(2)}`)
    .join(' '),
)

const yGridLines = [100, 75, 50, 25, 0]
const xTicks = computed(() => {
  const tickStepMs = visibleDays.value <= 1.5 ? 6 * 60 * 60 * 1000 : 24 * 60 * 60 * 1000
  const firstTick = Math.ceil(minTimestamp.value / tickStepMs) * tickStepMs
  const ticks: number[] = [minTimestamp.value]

  for (let tick = firstTick; tick < maxTimestamp.value; tick += tickStepMs) {
    if (tick > minTimestamp.value) {
      ticks.push(tick)
    }
  }

  ticks.push(maxTimestamp.value)
  return Array.from(new Set(ticks))
})

function xForTimestamp(timestamp: number): number {
  return ((timestamp - minTimestamp.value) / timestampRange.value) * plotWidth.value
}

function yForPercent(percent: number): number {
  return margin.top + ((100 - percent) / 100) * plotHeight
}

function segmentX(segment: BatterySegment): number {
  return xForTimestamp(segment.startTimestamp)
}

function segmentWidth(segment: BatterySegment): number {
  return Math.max(1, xForTimestamp(segment.endTimestamp) - xForTimestamp(segment.startTimestamp))
}

function formatAxisTime(timestamp: number): string {
  return new Date(timestamp).toLocaleString(undefined, {
    month: 'short',
    day: 'numeric',
    hour: '2-digit',
    minute: '2-digit',
  })
}

function formatTickTime(timestamp: number): string {
  return new Date(timestamp).toLocaleString(undefined, {
    month: 'short',
    day: 'numeric',
    ...(visibleDays.value <= 1.5 ? { hour: '2-digit' as const } : {}),
  })
}

function measureViewport() {
  if (graphScroller.value === null) {
    return
  }

  viewportWidth.value = Math.max(320, graphScroller.value.clientWidth)
}

function scrollActiveSegmentIntoView() {
  const scroller = graphScroller.value
  const activeSegment = props.segments.find((segment) => segment.id === props.activeSegmentId)

  if (scroller === null || activeSegment === undefined) {
    return
  }

  const segmentCenter = segmentX(activeSegment) + segmentWidth(activeSegment) / 2
  const scrollLeft = Math.max(0, segmentCenter - scroller.clientWidth / 2)

  scroller.scrollTo({
    left: scrollLeft,
    behavior: 'smooth',
  })
}

function clampVisibleDays(days: number): number {
  return Math.min(maxVisibleDays, Math.max(minVisibleDays, days))
}

function handleGraphWheel(event: WheelEvent) {
  const scroller = graphScroller.value

  if (scroller === null || Math.abs(event.deltaY) <= Math.abs(event.deltaX)) {
    return
  }

  event.preventDefault()

  const oldPlotWidth = plotWidth.value
  const scrollerRect = scroller.getBoundingClientRect()
  const pointerX = Math.min(scroller.clientWidth, Math.max(0, event.clientX - scrollerRect.left))
  const pointerTimelineRatio = (scroller.scrollLeft + pointerX) / oldPlotWidth
  const zoomDirection = event.deltaY > 0 ? 1 : -1
  const nextVisibleDays = clampVisibleDays(visibleDays.value + zoomDirection * 0.5)

  if (nextVisibleDays === visibleDays.value) {
    return
  }

  visibleDays.value = nextVisibleDays

  nextTick(() => {
    scroller.scrollLeft = Math.max(0, pointerTimelineRatio * plotWidth.value - pointerX)
  })
}

onMounted(() => {
  measureViewport()

  if (graphScroller.value !== null) {
    resizeObserver = new ResizeObserver(measureViewport)
    resizeObserver.observe(graphScroller.value)
  }
})

onBeforeUnmount(() => {
  resizeObserver?.disconnect()
})

watch(
  () => props.activeSegmentId,
  () => {
    nextTick(scrollActiveSegmentIntoView)
  },
)
</script>

<template>
  <section class="graph-panel" aria-label="Battery percentage graph">
    <div class="graph-header">
      <div>
        <h2>Battery status</h2>
        <p>{{ formatAxisTime(minTimestamp) }} - {{ formatAxisTime(maxTimestamp) }}</p>
      </div>
      <div class="legend">
        <span class="legend-item legend-charging">Charging</span>
        <span class="legend-item legend-discharging">Discharging</span>
      </div>
      <label class="zoom-control">
        <span>Zoom</span>
        <input
          v-model.number="visibleDays"
          type="range"
          :min="minVisibleDays"
          :max="maxVisibleDays"
          step="0.5"
        />
        <strong>{{ visibleDays }}d</strong>
      </label>
    </div>

    <div class="graph-body">
      <svg
        class="y-axis"
        :width="axisWidth"
        :height="height"
        :viewBox="`0 0 ${axisWidth} ${height}`"
        aria-hidden="true"
      >
        <g v-for="percent in yGridLines" :key="percent">
          <text :x="axisWidth - 10" :y="yForPercent(percent) + 4">{{ percent }}%</text>
        </g>
      </svg>

      <div ref="graphScroller" class="graph-scroller" @wheel="handleGraphWheel">
        <svg
          class="battery-graph"
          :style="{ width: `${svgWidth}px` }"
          :viewBox="`0 0 ${svgWidth} ${height}`"
          role="img"
        >
          <rect
            v-for="segment in segments"
            :key="segment.id"
            class="segment-band"
            :class="[
              segment.plugged ? 'band-charging' : 'band-discharging',
              { 'band-active': segment.id === activeSegmentId },
            ]"
            :x="segmentX(segment)"
            :y="margin.top"
            :width="segmentWidth(segment)"
            :height="plotHeight"
            @click="emit('activateSegment', segment.id)"
          >
            <title>
              {{ segment.plugged ? 'Charging' : 'Discharging' }}:
              {{ segment.startPercent }}% to {{ segment.endPercent }}%
            </title>
          </rect>

          <g class="grid-lines">
            <g v-for="percent in yGridLines" :key="percent">
              <line
                :x1="0"
                :x2="plotWidth"
                :y1="yForPercent(percent)"
                :y2="yForPercent(percent)"
              />
            </g>
          </g>

          <polyline class="battery-line" :points="linePoints" />

          <rect
            v-for="segment in segments.filter((item) => item.id === activeSegmentId)"
            :key="`active-${segment.id}`"
            class="active-outline"
            :x="segmentX(segment)"
            :y="margin.top"
            :width="segmentWidth(segment)"
            :height="plotHeight"
          />

          <line
            class="axis-line"
            :x1="0"
            :x2="plotWidth"
            :y1="height - margin.bottom"
            :y2="height - margin.bottom"
          />

          <g class="x-axis">
            <g v-for="tick in xTicks" :key="tick">
              <line
                :x1="xForTimestamp(tick)"
                :x2="xForTimestamp(tick)"
                :y1="height - margin.bottom"
                :y2="height - margin.bottom + 6"
              />
              <text :x="xForTimestamp(tick)" :y="height - 9">{{ formatTickTime(tick) }}</text>
            </g>
          </g>
        </svg>
      </div>
    </div>
  </section>
</template>

<style scoped>
.graph-panel {
  position: sticky;
  top: 0;
  z-index: 5;
  max-width: 1120px;
  margin: 0 auto 18px;
  border: 1px solid #d9dee7;
  border-radius: 8px;
  background: rgba(255, 255, 255, 0.96);
  box-shadow: 0 12px 26px rgba(28, 37, 48, 0.08);
  overflow: hidden;
}

.graph-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 16px;
  padding: 14px 18px 8px;
}

.graph-header h2,
.graph-header p {
  margin: 0;
}

.graph-header h2 {
  font-size: 18px;
}

.graph-header p {
  color: #687383;
  font-size: 13px;
}

.legend {
  display: flex;
  gap: 10px;
}

.legend-item {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  color: #4d5968;
  font-size: 12px;
  font-weight: 700;
}

.legend-item::before {
  display: block;
  width: 14px;
  height: 8px;
  border-radius: 999px;
  content: "";
}

.legend-charging::before {
  background: #dff4e5;
}

.legend-discharging::before {
  background: #fff0d8;
}

.zoom-control {
  display: grid;
  grid-template-columns: auto minmax(120px, 180px) 32px;
  align-items: center;
  gap: 8px;
  color: #4d5968;
  font-size: 12px;
  font-weight: 700;
}

.zoom-control input {
  accent-color: #24364b;
}

.zoom-control strong {
  color: #1c2530;
  font-size: 12px;
  text-align: right;
}

.graph-body {
  position: relative;
  padding-left: 48px;
}

.y-axis {
  position: absolute;
  top: 0;
  left: 0;
  z-index: 2;
  background: linear-gradient(90deg, #ffffff 82%, rgba(255, 255, 255, 0));
}

.y-axis text {
  fill: #687383;
  font-size: 12px;
  text-anchor: end;
}

.graph-scroller {
  overflow-x: auto;
  overflow-y: hidden;
}

.battery-graph {
  display: block;
  height: 240px;
}

.segment-band {
  cursor: pointer;
  transition:
    opacity 120ms ease,
    fill 120ms ease;
}

.band-charging {
  fill: #dff4e5;
  opacity: 0.72;
}

.band-discharging {
  fill: #fff0d8;
  opacity: 0.78;
}

.band-active {
  opacity: 1;
}

.grid-lines line {
  stroke: #d9dee7;
  stroke-width: 1;
}

.battery-line {
  fill: none;
  stroke: #24364b;
  stroke-linecap: round;
  stroke-linejoin: round;
  stroke-width: 3;
}

.active-outline {
  fill: none;
  pointer-events: none;
  stroke: #24364b;
  stroke-dasharray: 6 4;
  stroke-width: 2;
}

.axis-line,
.x-axis line {
  stroke: #9ca7b6;
  stroke-width: 1.5;
}

.x-axis text {
  fill: #687383;
  font-size: 12px;
  text-anchor: middle;
}

@media (max-width: 760px) {
  .graph-panel {
    margin-bottom: 14px;
  }

  .graph-header {
    align-items: flex-start;
    flex-direction: column;
  }

  .zoom-control {
    width: 100%;
    grid-template-columns: auto minmax(120px, 1fr) 32px;
  }

  .battery-graph {
    height: 210px;
  }
}
</style>
