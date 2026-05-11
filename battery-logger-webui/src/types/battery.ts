export type BatteryLogRow = {
  time: string
  timestamp: number
  plugged: boolean
  percent: number
  machineId: string
}

export type BatterySegment = {
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
