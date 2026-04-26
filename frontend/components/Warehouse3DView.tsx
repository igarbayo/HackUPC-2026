'use client'

import { useEffect, useRef, useState } from 'react'
import * as THREE from 'three'
import type { TickSnapshot } from '@/lib/SimulationStreamContext'
import { FF } from '@/lib/tokens'

// ── Internal types ────────────────────────────────────────────────────

interface SceneConfig {
  aisles:      number
  robots:      number
  depth:       number   // num_slots (X)
  levels:      number   // num_y (Y)
  sides:       number   // num_sides (1 or 2)
  aisleWidth:  number
  rackDepth:   number
  levelHeight: number
  postSize:    number
  DL:          number   // (depth - 1) * 0.9
}

interface ShuttleEntry {
  mesh:     THREE.Group
  li:       number
  worldZ:   number
  carrying: boolean
  baseY:    number
}

// boxGrid[level][side][slot][zLayer]
type BoxGrid = (THREE.Mesh | null)[][][][]

interface AisleData {
  group:    THREE.Group
  shuttles: ShuttleEntry[]
  boxGrid:  BoxGrid
}

// ── SiloScene ─────────────────────────────────────────────────────────

const AISLE_GAP = 0.8

class SiloScene {
  private canvas: HTMLCanvasElement
  private cfg: SceneConfig
  private alive = true

  private renderer!: THREE.WebGLRenderer
  private scene!:    THREE.Scene
  private camera!:   THREE.PerspectiveCamera
  private M!: Record<string, THREE.MeshLambertMaterial>
  private edgeMat!: THREE.LineBasicMaterial

  private orb      = { theta: Math.PI, phi: 0.28, r: 16, tTheta: Math.PI, tPhi: 0.28, tR: 16 }
  private camTarget = new THREE.Vector3(0, 3, 0)
  private camGoal   = new THREE.Vector3(0, 3, 0)
  private drag      = { active: false, x: 0, y: 0 }

  private aisleData:       AisleData[]   = []
  private palletBoxMeshes: THREE.Mesh[]  = []
  private armUs:           THREE.Mesh[]  = []
  private armT = 0

  private _ro?:     ResizeObserver
  private _rafId?:  number

  private _onMouseUp   = () => { this.drag.active = false }
  private _onMouseMove = (e: MouseEvent) => {
    if (!this.drag.active) return
    const dx = e.clientX - this.drag.x
    const dy = e.clientY - this.drag.y
    this.drag.x = e.clientX
    this.drag.y = e.clientY
    this.orb.tTheta -= dx * 0.004
    this.orb.tPhi = Math.max(0.06, Math.min(1.45, this.orb.tPhi - dy * 0.004))
  }

  constructor(canvas: HTMLCanvasElement, cfg: SceneConfig) {
    this.canvas = canvas
    this.cfg    = cfg
    this._init()
    this._bindEvents()
    this._loop()
  }

  // ── Init ─────────────────────────────────────────────────────────────

  private _init() {
    this.renderer = new THREE.WebGLRenderer({ canvas: this.canvas, antialias: true })
    this.renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2))
    this.renderer.shadowMap.enabled = true
    this.renderer.shadowMap.type    = THREE.PCFSoftShadowMap
    this.renderer.toneMapping       = THREE.ACESFilmicToneMapping
    this.renderer.toneMappingExposure = 1.1
    this._resize()

    this.scene = new THREE.Scene()
    this.scene.background = new THREE.Color(0xf8f7f5)
    this.scene.fog         = new THREE.FogExp2(0xf8f7f5, 0.014)

    this.camera = new THREE.PerspectiveCamera(42, 1, 0.1, 300)
    this._applyCamPos()

    this.scene.add(new THREE.AmbientLight(0xffffff, 0.55))

    const sun = new THREE.DirectionalLight(0xfff5e8, 1.1)
    sun.position.set(18, 35, 12)
    sun.castShadow = true
    sun.shadow.mapSize.set(2048, 2048)
    Object.assign(sun.shadow.camera, { left: -60, right: 60, top: 60, bottom: -60, near: 1, far: 180 })
    this.scene.add(sun)

    const fill = new THREE.DirectionalLight(0xd0e8ff, 0.35)
    fill.position.set(-15, 12, -8)
    this.scene.add(fill)

    this.scene.add(new THREE.HemisphereLight(0xffffff, 0xd8cfc0, 0.25))

    this.edgeMat = new THREE.LineBasicMaterial({ color: 0x000000, transparent: true, opacity: 0.55 })

    this.M = {
      rack:      new THREE.MeshLambertMaterial({ color: 0x9e9e9e }),
      rackDk:    new THREE.MeshLambertMaterial({ color: 0x6e6e6e }),
      rail:      new THREE.MeshLambertMaterial({ color: 0xbdbdbd }),
      box:       new THREE.MeshLambertMaterial({ color: 0xc9a96e }),
      shuttle:   new THREE.MeshLambertMaterial({ color: 0xd8d8d8 }),
      shuttleHL: new THREE.MeshLambertMaterial({ color: 0x5ba3f5, emissive: new THREE.Color(0x1a4a8a), emissiveIntensity: 0.15 }),
      robot:     new THREE.MeshLambertMaterial({ color: 0x1c1c1c }),
      robotAcc:  new THREE.MeshLambertMaterial({ color: 0xf5a623, emissive: new THREE.Color(0x603000), emissiveIntensity: 0.2 }),
      pallet:    new THREE.MeshLambertMaterial({ color: 0x7a5c1e }),
      floor:     new THREE.MeshLambertMaterial({ color: 0xe4e2dc }),
      ceiling:   new THREE.MeshLambertMaterial({ color: 0xd8d6d0, transparent: true, opacity: 0.35 }),
    }

    this._buildWarehouse()
  }

  // ── Geometry helpers ─────────────────────────────────────────────────

  private _withEdges(mesh: THREE.Mesh): THREE.Mesh {
    mesh.add(new THREE.LineSegments(new THREE.EdgesGeometry(mesh.geometry), this.edgeMat))
    return mesh
  }

  private _box(
    w: number, h: number, d: number,
    mat: THREE.Material,
    px = 0, py = 0, pz = 0,
    parent: THREE.Object3D | null = null,
  ): THREE.Mesh {
    const m = new THREE.Mesh(new THREE.BoxGeometry(w, h, d), mat)
    m.position.set(px, py, pz)
    m.castShadow = true
    m.receiveShadow = true
    ;(parent ?? this.scene).add(m)
    return m
  }

  // ── Warehouse construction ────────────────────────────────────────────

  private _buildWarehouse() {
    const { cfg, M } = this
    const AS     = cfg.aisleWidth + cfg.rackDepth * cfg.sides
    const TH     = cfg.levels * cfg.levelHeight
    const DL     = cfg.DL
    const totalW = AS * cfg.aisles + AISLE_GAP * (cfg.aisles - 1)

    const floor = this._box(totalW + 8, 0.12, DL + 10, M.floor, 0, -0.06, 0)
    floor.receiveShadow = true

    const grid = new THREE.GridHelper(Math.max(totalW, DL) + 14, Math.max(totalW, DL) + 14, 0xc8c6c0, 0xc8c6c0)
    const gridMats = Array.isArray(grid.material) ? grid.material : [grid.material]
    gridMats.forEach(m => { m.transparent = true; m.opacity = 0.25 })
    this.scene.add(grid)

    this._box(totalW + 2, 0.08, DL + 2, M.ceiling, 0, TH + 0.5, 0)

    this.aisleData = []
    for (let ai = 0; ai < cfg.aisles; ai++) {
      const offsetX = (ai - (cfg.aisles - 1) / 2) * (AS + AISLE_GAP)
      const ag = new THREE.Group()
      ag.position.x = offsetX
      this.scene.add(ag)

      const shuttles: ShuttleEntry[] = []
      this._buildAisle(ag, DL, shuttles)
      const boxGrid = this._buildBoxGrid(ag, DL)

      this.aisleData.push({ group: ag, shuttles, boxGrid })
    }

    this._buildRobots(DL, totalW)
  }

  private _buildAisle(g: THREE.Group, DL: number, shuttles: ShuttleEntry[]) {
    const { cfg, M } = this
    const HA    = cfg.aisleWidth / 2
    const RD    = cfg.rackDepth
    const TH    = cfg.levels * cfg.levelHeight
    const PS    = cfg.postSize
    const nCols  = Math.floor(cfg.depth / 2) + 1
    const safeZ  = (ci: number) => nCols === 1 ? 0 : -DL / 2 + ci * (DL / (nCols - 1))
    const minDL  = Math.max(DL, PS)

    for (let ci = 0; ci < nCols; ci++) {
      const z        = safeZ(ci)
      const postXs: [number, number][] = [[-HA - RD, z], [-HA, z]]
      if (cfg.sides > 1) postXs.push([HA, z], [HA + RD, z])
      for (const [px, pz] of postXs) {
        const post = new THREE.Mesh(new THREE.BoxGeometry(PS, TH, PS), M.rack)
        post.position.set(px, TH / 2, pz)
        post.castShadow = true
        g.add(post)
      }
    }

    for (let l = 0; l <= cfg.levels; l++) {
      const y = l * cfg.levelHeight
      const beamH = 0.065, beamW = 0.055
      const bGeo  = new THREE.BoxGeometry(beamW, beamH, minDL)
      const beamXs: number[] = [-HA - RD + 0.04, -HA - 0.04]
      if (cfg.sides > 1) beamXs.push(HA + 0.04, HA + RD - 0.04)
      for (const px of beamXs) {
        const b = new THREE.Mesh(bGeo, M.rackDk)
        b.position.set(px, y, 0)
        g.add(b)
      }
      if (l < cfg.levels) {
        for (let ci = 0; ci < nCols; ci++) {
          const z  = -DL / 2 + ci * (DL / (nCols - 1))
          const lb = new THREE.Mesh(new THREE.BoxGeometry(RD, beamH, beamW), M.rack)
          lb.position.set(-HA - RD / 2, y, z)
          g.add(lb)
          if (cfg.sides > 1) {
            const rb = new THREE.Mesh(new THREE.BoxGeometry(RD, beamH, beamW), M.rack)
            rb.position.set(HA + RD / 2, y, z)
            g.add(rb)
          }
        }
      }
    }

    for (let l = 0; l < cfg.levels; l++) {
      const ry     = l * cfg.levelHeight + 0.04
      const railGeo = new THREE.BoxGeometry(0.045, 0.03, minDL)
      for (const rx of [-0.38, 0.38]) {
        const rail = new THREE.Mesh(railGeo, M.rail)
        rail.position.set(rx, ry, 0)
        g.add(rail)
      }
      this._buildShuttle(g, l, shuttles)
    }
  }

  private _buildShuttle(g: THREE.Group, li: number, shuttles: ShuttleEntry[]) {
    const { cfg, M } = this
    const sg = new THREE.Group()

    sg.add(new THREE.Mesh(new THREE.BoxGeometry(0.82, 0.13, 0.68), M.shuttle))
    for (const rx of [-0.4, 0.4]) {
      const sk = new THREE.Mesh(new THREE.BoxGeometry(0.06, 0.18, 0.64), M.rackDk)
      sk.position.set(rx, 0, 0)
      sg.add(sk)
    }
    const bump = new THREE.Mesh(new THREE.BoxGeometry(0.78, 0.04, 0.1), M.rackDk)
    bump.position.set(0, 0.085, 0)
    sg.add(bump)
    this._box(0.55, 0.04, 0.08, M.rackDk, 0, 0.09, 0, sg)

    const baseY = li * cfg.levelHeight + 0.13
    sg.position.set(0, baseY, 0)
    g.add(sg)
    shuttles.push({ mesh: sg, li, worldZ: 0, carrying: false, baseY })
  }

  private _buildBoxGrid(g: THREE.Group, DL: number): BoxGrid {
    const { cfg, M } = this
    const HA    = cfg.aisleWidth / 2
    const RD    = cfg.rackDepth
    const BW    = RD / 2 - 0.05
    const BH    = 0.48
    const BD    = 0.68
    const slotZ = DL / cfg.depth

    return Array.from({ length: cfg.levels }, (_, li) =>
      Array.from({ length: cfg.sides }, (_, si) =>
        Array.from({ length: cfg.depth }, (_, di) =>
          Array.from({ length: 2 }, (_, zLayer) => {
            const y       = li * cfg.levelHeight + BH / 2 + 0.03
            const z       = -DL / 2 + (di + 0.5) * slotZ
            const xOffset = zLayer === 0 ? RD * 0.25 : RD * 0.75
            const x       = si === 0 ? -HA - xOffset : HA + xOffset
            const mesh    = this._withEdges(new THREE.Mesh(new THREE.BoxGeometry(BW, BH, BD), M.box))
            mesh.position.set(x, y, z)
            mesh.castShadow = true
            mesh.visible    = false
            g.add(mesh)
            return mesh
          })
        )
      )
    ) as BoxGrid
  }

  private _buildRobots(DL: number, totalW: number) {
    const { cfg } = this
    const frontZ = -DL / 2 - 1.8

    this.palletBoxMeshes = []
    this.armUs           = []

    const xs = cfg.robots === 1 ? [0] : [-(totalW / 4 + 0.3), totalW / 4 + 0.3]
    xs.forEach((rx, ri) => {
      const rg = this._buildRobotGroup(ri === 0)
      rg.position.set(rx, 0, frontZ)
      this.scene.add(rg)
    })
  }

  private _buildRobotGroup(withPallet: boolean): THREE.Group {
    const { M } = this
    const rg = new THREE.Group()

    this._box(0.55, 0.18, 0.55, M.rack,     0, 0.09, 0, rg)
    this._box(0.42, 0.9,  0.4,  M.robot,    0, 0.63, 0, rg)
    this._box(0.42, 0.06, 0.42, M.robotAcc, 0, 1.05, 0, rg)
    this._box(0.28, 0.24, 0.28, M.rackDk,   0, 1.23, 0, rg)

    const armU = this._box(0.09, 0.52, 0.09, M.robotAcc, 0.28, 0.9, 0, rg)
    this.armUs.push(armU)
    this._box(0.09, 0.09, 0.38, M.robot, 0.28, 0.62, 0, rg)
    for (const gz of [-0.1, 0.1]) {
      this._box(0.07, 0.04, 0.14, M.robot, 0.28, 0.44, gz, rg)
    }

    if (withPallet) {
      // 4 pallets arranged 2×2
      const PALLET_SLOTS = 4
      const palletPositions = [
        { ox: -0.58, oz: -0.48 },
        { ox: -0.58, oz:  0.48 },
        { ox:  0.52, oz: -0.48 },
        { ox:  0.52, oz:  0.48 },
      ]
      for (let pi = 0; pi < PALLET_SLOTS; pi++) {
        const { ox, oz } = palletPositions[pi]
        this._box(1.0, 0.12, 0.88, M.pallet, ox, 0.06, oz, rg)
        for (let s = 0; s < 3; s++) {
          this._box(1.0, 0.08, 0.06, M.rackDk, ox, 0.02, oz - 0.3 + s * 0.3, rg)
        }
        // 3 rows × 2 cols = 6 boxes per pallet (visual only; real capacity = 12)
        for (let row = 0; row < 3; row++) {
          for (let col = 0; col < 2; col++) {
            const bx = this._withEdges(new THREE.Mesh(new THREE.BoxGeometry(0.44, 0.38, 0.38), M.box))
            bx.position.set(ox + (col - 0.5) * 0.46, 0.18 + row * 0.38, oz)
            bx.castShadow = true
            bx.visible    = false
            rg.add(bx)
            this.palletBoxMeshes.push(bx)
          }
        }
      }
    }

    return rg
  }

  // ── Snapshot → scene ─────────────────────────────────────────────────

  applySnapshot(snapshot: TickSnapshot, depth: number) {
    const { DL, levels } = this.cfg

    for (let ai = 0; ai < this.cfg.aisles; ai++) {
      const aisleState = snapshot.aisles[ai]
      const data       = this.aisleData[ai]
      if (!aisleState || !data) continue

      for (const sh of aisleState.shuttles) {
        const li    = sh.y_level - 1
        const entry = data.shuttles[li]
        if (entry) {
          entry.worldZ = -DL / 2 + (sh.position.x + 0.5) * (DL / depth)
          entry.carrying = sh.is_carrying
        }

        if (li < 0 || li >= levels) continue

        for (let si = 0; si < this.cfg.sides; si++) {
          for (let di = 0; di < depth; di++) {
            for (let zl = 0; zl < 2; zl++) {
              const m = data.boxGrid[li]?.[si]?.[di]?.[zl]
              if (m) m.visible = false
            }
          }
        }
        for (const box of sh.floor_boxes) {
          if (!box.position) continue
          const si     = box.position.side - 1
          const di     = box.position.x
          const zLayer = box.position.z - 1
          if (si >= 0 && si < this.cfg.sides && di >= 0 && di < depth && zLayer >= 0 && zLayer < 2) {
            const m = data.boxGrid[li]?.[si]?.[di]?.[zLayer]
            if (m) m.visible = true
          }
        }
      }
    }

    // Distribute boxes across the 4 pallet slots (6 visual boxes each).
    // Use p.slot (0-3) as the authoritative visual position — C++ skips null
    // slots in the snapshot array, so array index != slot index.
    this.palletBoxMeshes.forEach(m => { m.visible = false })
    for (const p of snapshot.robot.pallets) {
      const base = (p.slot - 1) * 6
      for (let bi = 0; bi < Math.min(p.placed_count, 6); bi++) {
        const m = this.palletBoxMeshes[base + bi]
        if (m) m.visible = true
      }
    }
  }

  // ── Render loop ───────────────────────────────────────────────────────

  private _renderShuttles() {
    for (const { shuttles } of this.aisleData) {
      for (const sh of shuttles) {
        sh.mesh.position.z = sh.worldZ
        sh.mesh.position.y = sh.baseY
        ;(sh.mesh.children[0] as THREE.Mesh).material = sh.carrying ? this.M.shuttleHL : this.M.shuttle
      }
    }
  }

  private _renderRobot() {
    this.armT += 0.016
    for (const armU of this.armUs) {
      armU.position.y = 0.9 + Math.sin(this.armT * 0.7) * 0.18
      armU.rotation.z = Math.sin(this.armT * 0.5) * 0.3
    }
  }

  private _applyCamPos() {
    const { orb, camTarget, camera } = this
    const x = camTarget.x + orb.r * Math.sin(orb.theta) * Math.cos(orb.phi)
    const y = camTarget.y + orb.r * Math.sin(orb.phi)
    const z = camTarget.z + orb.r * Math.cos(orb.theta) * Math.cos(orb.phi)
    camera.position.set(x, y, z)
    camera.lookAt(camTarget)
  }

  private _loop() {
    if (!this.alive) return
    this._rafId = requestAnimationFrame(() => this._loop())
    const LS = 0.055
    this.orb.theta += (this.orb.tTheta - this.orb.theta) * LS
    this.orb.phi   += (this.orb.tPhi   - this.orb.phi)   * LS
    this.orb.r     += (this.orb.tR     - this.orb.r)     * LS
    this.camTarget.lerp(this.camGoal, LS)
    this._applyCamPos()
    this._renderShuttles()
    this._renderRobot()
    this.renderer.render(this.scene, this.camera)
  }

  // ── Camera views ─────────────────────────────────────────────────────

  setView(view: 'silo' | 'aisle' | 'shuttle') {
    const { cfg } = this
    const AS     = cfg.aisleWidth + cfg.rackDepth * cfg.sides
    const totalW = AS * cfg.aisles + AISLE_GAP * (cfg.aisles - 1)
    const midY   = (cfg.levels * cfg.levelHeight) / 2

    if (view === 'silo') {
      this.orb.tTheta = 0.6
      this.orb.tPhi   = 0.42
      this.orb.tR     = Math.max(20, cfg.levels * 3 + cfg.depth * 0.4 + totalW * 1.2)
      this.camGoal.set(0, midY, 0)
    } else if (view === 'aisle') {
      this.orb.tTheta = Math.PI
      this.orb.tPhi   = 0.28
      this.orb.tR     = Math.max(16, 10 + totalW * 0.8)
      this.camGoal.set(0, midY, 0)
    } else {
      const sh = this.aisleData[0]?.shuttles[0]
      this.orb.tTheta = 0.2
      this.orb.tPhi   = 0.18
      this.orb.tR     = 5.5
      this.camGoal.set(0, (sh?.baseY ?? midY) + 0.5, sh?.worldZ ?? 0)
    }
  }

  // ── Events & cleanup ─────────────────────────────────────────────────

  private _resize() {
    const { canvas, renderer } = this
    const w = canvas.clientWidth, h = canvas.clientHeight
    if (!w || !h) return
    renderer.setSize(w, h, false)
    if (this.camera) {
      this.camera.aspect = w / h
      this.camera.updateProjectionMatrix()
    }
  }

  private _bindEvents() {
    const { canvas } = this
    canvas.addEventListener('mousedown', e => {
      this.drag.active = true
      this.drag.x = e.clientX
      this.drag.y = e.clientY
    })
    window.addEventListener('mouseup',   this._onMouseUp)
    window.addEventListener('mousemove', this._onMouseMove)
    canvas.addEventListener('wheel', e => {
      e.preventDefault()
      this.orb.tR = Math.max(2.5, Math.min(100, this.orb.tR + e.deltaY * 0.04))
    }, { passive: false })

    let lastTouch: { x: number; y: number } | null = null
    canvas.addEventListener('touchstart', e => {
      if (e.touches.length === 1) lastTouch = { x: e.touches[0].clientX, y: e.touches[0].clientY }
    })
    canvas.addEventListener('touchmove', e => {
      e.preventDefault()
      if (e.touches.length === 1 && lastTouch) {
        const dx = e.touches[0].clientX - lastTouch.x
        const dy = e.touches[0].clientY - lastTouch.y
        this.orb.tTheta -= dx * 0.005
        this.orb.tPhi = Math.max(0.06, Math.min(1.45, this.orb.tPhi - dy * 0.005))
        lastTouch = { x: e.touches[0].clientX, y: e.touches[0].clientY }
      }
    }, { passive: false })

    this._ro = new ResizeObserver(() => this._resize())
    this._ro.observe(canvas)
    setTimeout(() => this._resize(), 0)
  }

  dispose() {
    this.alive = false
    if (this._rafId !== undefined) cancelAnimationFrame(this._rafId)
    this._ro?.disconnect()
    window.removeEventListener('mouseup',   this._onMouseUp)
    window.removeEventListener('mousemove', this._onMouseMove)
    this.scene.traverse(obj => {
      if (obj instanceof THREE.Mesh) obj.geometry.dispose()
    })
    Object.values(this.M).forEach(m => m.dispose())
    this.edgeMat.dispose()
    this.renderer.dispose()
    this.aisleData       = []
    this.palletBoxMeshes = []
    this.armUs           = []
  }
}

// ── React component ───────────────────────────────────────────────────

interface Props {
  simId: string
  params: {
    num_slots:  number
    num_y:      number
    num_sides:  number
    num_aisles: number
    num_robots: number
  }
  snapshot: TickSnapshot | null
}

export default function Warehouse3DView({ simId: _simId, params, snapshot }: Props) {
  const canvasRef = useRef<HTMLCanvasElement>(null)
  const sceneRef  = useRef<SiloScene | null>(null)
  const depthRef  = useRef(params.num_slots)
  const [view, setView] = useState<'silo' | 'aisle' | 'shuttle'>('aisle')

  useEffect(() => {
    if (!canvasRef.current) return
    const cfg: SceneConfig = {
      aisles:      params.num_aisles,
      robots:      params.num_robots,
      depth:       params.num_slots,
      levels:      params.num_y,
      sides:       params.num_sides,
      aisleWidth:  1.5,
      rackDepth:   0.85,
      levelHeight: 1.35,
      postSize:    0.07,
      DL:          Math.max(0.9, (params.num_slots - 1) * 0.9),
    }
    depthRef.current = params.num_slots
    const scene = new SiloScene(canvasRef.current, cfg)
    scene.setView('aisle')
    sceneRef.current = scene
    return () => { scene.dispose(); sceneRef.current = null }
  }, [params.num_slots, params.num_y, params.num_sides, params.num_aisles, params.num_robots])

  useEffect(() => {
    if (!snapshot || !sceneRef.current) return
    sceneRef.current.applySnapshot(snapshot, depthRef.current)
  }, [snapshot])

  useEffect(() => {
    sceneRef.current?.setView(view)
  }, [view])

  return (
    <div style={{ position: 'relative', width: '100%', height: '100%' }}>
      <div style={{ position: 'absolute', top: 12, left: 12, zIndex: 10, display: 'flex', gap: 6 }}>
        {(['silo', 'aisle', 'shuttle'] as const).map(v => (
          <button
            key={v}
            onClick={() => setView(v)}
            style={{
              background:    view === v ? '#000' : 'rgba(255,255,255,0.88)',
              color:         view === v ? '#fff' : '#555',
              border:        '1px solid ' + (view === v ? '#000' : '#e0e0e0'),
              padding:       '5px 12px',
              fontSize:      9,
              letterSpacing: '0.16em',
              textTransform: 'uppercase',
              fontFamily:    FF,
              cursor:        'pointer',
              transition:    'all 0.15s',
            }}
          >
            {v === 'silo' ? 'Warehouse' : v === 'aisle' ? 'Aisle' : 'Shuttle'}
          </button>
        ))}
      </div>
      <div style={{
        position: 'absolute', bottom: 12, left: 12, zIndex: 10,
        fontSize: 9, letterSpacing: '0.1em', textTransform: 'uppercase',
        color: 'rgba(0,0,0,0.3)', fontFamily: FF, pointerEvents: 'none',
      }}>
        Drag · Scroll to zoom
      </div>
      <canvas
        ref={canvasRef}
        style={{ display: 'block', width: '100%', height: '100%', cursor: 'grab' }}
      />
    </div>
  )
}
