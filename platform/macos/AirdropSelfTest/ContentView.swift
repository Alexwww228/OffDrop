// ContentView.swift
// Главный экран: две секции, обе показывают РЕАЛЬНОЕ состояние, не заглушки.
//   1. Self-test C++ core — доказывает, что крипто/протокол работают.
//   2. Bluetooth — доказывает, что устройство реально видно другим и видит других.

import SwiftUI

struct ContentView: View {
    @State private var selfTestSteps: [SelfTestStep] = []
    @State private var selfTestRan = false
    @StateObject private var bluetooth = BluetoothManager()

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 20) {
                Text("offline-airdrop — проверка работоспособности")
                    .font(.title2)
                    .bold()

                selfTestSection
                Divider()
                bluetoothSection
            }
            .padding(24)
        }
        .frame(minWidth: 480, minHeight: 520)
        .onAppear {
            runSelfTest()
        }
    }

    private var selfTestSection: some View {
        VStack(alignment: .leading, spacing: 10) {
            HStack {
                Text("1. C++ core (крипто, протокол)")
                    .font(.headline)
                Spacer()
                Button("Перезапустить") { runSelfTest() }
            }

            if !selfTestRan {
                ProgressView("Выполняется self-test...")
            } else {
                ForEach(selfTestSteps) { step in
                    HStack(alignment: .top, spacing: 8) {
                        Image(systemName: step.success ? "checkmark.circle.fill" : "xmark.circle.fill")
                            .foregroundColor(step.success ? .green : .red)
                        VStack(alignment: .leading, spacing: 2) {
                            Text(step.title).bold()
                            Text(step.detail)
                                .font(.caption)
                                .foregroundColor(.secondary)
                        }
                    }
                }

                let allOk = selfTestSteps.allSatisfy { $0.success }
                Text(allOk ? "Все проверки прошли — core работает корректно." : "Есть провалившиеся проверки.")
                    .font(.subheadline)
                    .foregroundColor(allOk ? .green : .red)
                    .padding(.top, 4)
            }
        }
    }

    private var bluetoothSection: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("2. Bluetooth (реальный CoreBluetooth)")
                .font(.headline)

            Text(bluetooth.statusMessage)
                .font(.caption)
                .foregroundColor(.secondary)

            HStack(spacing: 12) {
                Button(bluetooth.isAdvertising ? "Остановить объявление" : "Начать объявление") {
                    bluetooth.isAdvertising ? bluetooth.stopAdvertising() : bluetooth.startAdvertising()
                }
                .disabled(!bluetooth.isBluetoothPoweredOn)

                Button(bluetooth.isScanning ? "Остановить сканирование" : "Начать сканирование") {
                    bluetooth.isScanning ? bluetooth.stopScanning() : bluetooth.startScanning()
                }
                .disabled(!bluetooth.isBluetoothPoweredOn)
            }

            if bluetooth.isScanning {
                Text("Найденные устройства поблизости:")
                    .font(.subheadline)
                    .padding(.top, 6)

                if bluetooth.discoveredPeers.isEmpty {
                    Text("Пока никого — запусти это же приложение на втором Mac рядом и нажми «Начать объявление» там.")
                        .font(.caption)
                        .foregroundColor(.secondary)
                } else {
                    ForEach(bluetooth.discoveredPeers) { peer in
                        HStack {
                            Image(systemName: "dot.radiowaves.left.and.right")
                            Text(peer.name)
                            Spacer()
                            Text("RSSI \(peer.rssi)")
                                .font(.caption)
                                .foregroundColor(.secondary)
                        }
                    }
                }
            }

            Text("Передача файлов через это соединение — следующий шаг реализации (см. platform/macos/README.md).")
                .font(.caption)
                .foregroundColor(.secondary)
                .padding(.top, 8)
        }
    }

    private func runSelfTest() {
        selfTestRan = false
        DispatchQueue.global(qos: .userInitiated).async {
            let steps = CoreSelfTest.run()
            DispatchQueue.main.async {
                selfTestSteps = steps
                selfTestRan = true
            }
        }
    }
}

#Preview {
    ContentView()
}
