#define _CRT_SECURE_NO_WARNINGS
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <string>
#include <vector>
#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>

using namespace std;

// ============================================================
//  STRUKTUR DATA
// ============================================================
struct DataKursi {
    int    status = 0;
    string nama = "";
    string idBooking = "";
};

struct Bus {
    string rute, kelas, kodePrefix, jamBerangkat, fasilitas;
    int    harga = 0, jumlahBaris = 0, jumlahKolom = 0,
        totalKursi = 0, formatKiri = 0;
    DataKursi kursi[10][5];
};

struct KursiDipilih { int row, col; };

// ============================================================
//  STATE
// ============================================================
enum AppState {
    STATE_HOME = 0,
    STATE_PILIH_RUTE,
    STATE_PILIH_KURSI,
    STATE_KONFIRMASI,
    STATE_SUKSES,
    STATE_CEK_TIKET
};

// ============================================================
//  GLOBAL
// ============================================================
Bus      daftarBus[9];
AppState appState = STATE_HOME;
int      busTerpilih = -1;
int      idCounter = 1001;

// Multi-kursi
vector<KursiDipilih> kursiTerpilih;   // daftar kursi yang dipilih

char   inputNama[128] = "";
char   inputIdCek[64] = "";
char   prevInputIdCek[64] = "";       // untuk deteksi perubahan input
string warningMsg = "";
vector<string> lastBookingIds;        // ID booking hasil pemesanan terakhir

// ============================================================
//  INISIALISASI BUS
// ============================================================
void InisialisasiDataBus() {
    struct Info {
        int idx; string r, k, p, j, fas; int h, brs, kol, f;
    };
    vector<Info> list = {
        {0,"Jakarta","Ekonomi",  "JKT-EKO","07:00","AC, Reclining Seat, Musik",              280000,10,5,2},
        {1,"Jakarta","Eksekutif","JKT-EXE","14:00","AC, Reclining Seat, USB, Snack",          480000,10,3,1},
        {2,"Jakarta","Sleeper",  "JKT-SLP","21:00","Full Flat Bed, Selimut, Bantal, Makan",   750000,10,2,1},
        {3,"Jogja",  "Ekonomi",  "JOG-EKO","07:00","AC, Reclining Seat, Musik",              150000,10,5,2},
        {4,"Jogja",  "Eksekutif","JOG-EXE","14:00","AC, Reclining Seat, USB, Snack",          250000,10,3,1},
        {5,"Jogja",  "Sleeper",  "JOG-SLP","21:00","Full Flat Bed, Selimut, Bantal, Makan",   380000,10,2,1},
        {6,"Bali",   "Ekonomi",  "BAL-EKO","07:00","AC, Reclining Seat, Musik",              220000,10,5,2},
        {7,"Bali",   "Eksekutif","BAL-EXE","14:00","AC, Reclining Seat, USB, Snack",          350000,10,3,1},
        {8,"Bali",   "Sleeper",  "BAL-SLP","21:00","Full Flat Bed, Selimut, Bantal, Makan",   550000,10,2,1},
    };
    for (auto& info : list) {
        Bus& b = daftarBus[info.idx];
        b.rute = info.r;  b.kelas = info.k;
        b.kodePrefix = info.p;  b.jamBerangkat = info.j;
        b.fasilitas = info.fas;b.harga = info.h;
        b.jumlahBaris = info.brs;b.jumlahKolom = info.kol;
        b.totalKursi = info.brs * info.kol;
        b.formatKiri = info.f;
        for (int i = 0;i < 10;i++) for (int j = 0;j < 5;j++) b.kursi[i][j] = { 0,"","" };
    }
}

// ============================================================
//  HELPER
// ============================================================
int HitungKursiTersedia(int idx) {
    Bus& b = daftarBus[idx]; int cnt = 0;
    for (int i = 0;i < b.jumlahBaris;i++)
        for (int j = 0;j < b.jumlahKolom;j++)
            if (b.kursi[i][j].status == 0) cnt++;
    return cnt;
}

bool KursiSudahDipilih(int row, int col) {
    for (auto& k : kursiTerpilih)
        if (k.row == row && k.col == col) return true;
    return false;
}

void ToggleKursi(int row, int col) {
    for (int i = 0;i < (int)kursiTerpilih.size();i++) {
        if (kursiTerpilih[i].row == row && kursiTerpilih[i].col == col) {
            kursiTerpilih.erase(kursiTerpilih.begin() + i);
            return;
        }
    }
    kursiTerpilih.push_back({ row, col });
}

string FormatRupiah(int angka) {
    string s = to_string(angka), hasil = ""; int cnt = 0;
    for (int i = (int)s.size() - 1;i >= 0;i--) {
        if (cnt && cnt % 3 == 0) hasil = "." + hasil;
        hasil = s[i] + hasil; cnt++;
    }
    return "Rp " + hasil;
}

struct SearchResult { bool found; int busIdx, row, col; };
SearchResult FindBookingById(const char* id) {
    string sid = id; if (sid.empty()) return { false };
    for (int b = 0;b < 9;b++)
        for (int i = 0;i < daftarBus[b].jumlahBaris;i++)
            for (int j = 0;j < daftarBus[b].jumlahKolom;j++)
                if (daftarBus[b].kursi[i][j].idBooking == sid)
                    return { true,b,i,j };
    return { false };
}

// ============================================================
//  RENDER PROGRESS STEPS
// ============================================================
void RenderProgressSteps(int step) {
    // step 1=Pilih Rute, 2=Pilih Kursi, 3=Konfirmasi
    const char* labels[] = { "1. Pilih Jadwal","2. Pilih Kursi","3. Konfirmasi" };
    ImGui::Spacing();
    for (int i = 0;i < 3;i++) {
        if (i < step - 1) ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.4f, 1), "[OK] %s", labels[i]);
        else if (i == step - 1)ImGui::TextColored(ImVec4(1, 0.85f, 0, 1), "[>>] %s", labels[i]);
        else                 ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "[ ] %s", labels[i]);
        if (i < 2) { ImGui::SameLine(); ImGui::TextDisabled("  |  "); ImGui::SameLine(); }
    }
    ImGui::NewLine();
    ImGui::Separator(); ImGui::Spacing();
}

// ============================================================
//  GLFW
// ============================================================
static void glfw_error_callback(int e, const char* d) {
    fprintf(stderr, "GLFW Error %d: %s\n", e, d);
}

// ============================================================
//  MAIN
// ============================================================
int main(int, char**) {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;

    const char* glsl_version = "#version 130";
    GLFWwindow* window = glfwCreateWindow(900, 700,
        "TixWay - Pesan Tiket Online", nullptr, nullptr);
    if (!window) return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = 1.1f;
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    InisialisasiDataBus();

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f; style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.ItemSpacing = ImVec2(10, 8);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.12f, 0.18f, 1.00f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.15f, 0.45f, 0.80f, 1.00f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.60f, 1.00f, 1.00f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.10f, 0.35f, 0.65f, 1.00f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.20f, 0.28f, 1.00f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.12f, 0.15f, 0.22f, 1.00f);

    // ================================================================
    //  MAIN LOOP
    // ================================================================
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        int W, H; glfwGetFramebufferSize(window, &W, &H);
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2((float)W, (float)H));
        ImGui::Begin("##root", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);

        // HEADER
        ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1),
            "[TixWay]");
        ImGui::SameLine(0, 20);
        ImGui::TextDisabled("Pesan tiket bus dengan mudah & cepat");
        ImGui::Separator(); ImGui::Spacing();

        // ============================================================
        //  HOME
        // ============================================================
        if (appState == STATE_HOME) {
            float cardW = 400;
            ImGui::SetCursorPosX(((float)W - cardW) / 2);
            ImGui::BeginChild("##home_card", ImVec2(cardW, 250), true);

            ImGui::Spacing();
            ImGui::SetCursorPosX(40);
            ImGui::TextColored(ImVec4(1, 0.85f, 0, 1),
                "Mau pergi ke mana hari ini?");
            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

            auto centeredBtn = [&](const char* label, float w, float h) -> bool {
                ImGui::SetCursorPosX((cardW - w) / 2);
                return ImGui::Button(label, ImVec2(w, h));
                };

            if (centeredBtn("Pesan Tiket Baru", 350, 60)) {
                appState = STATE_PILIH_RUTE;
                warningMsg = "";
            }
            ImGui::Spacing();
            if (centeredBtn("Cek / Batalkan Tiket Saya", 350, 60)) {
                appState = STATE_CEK_TIKET;
                memset(inputIdCek, 0, sizeof(inputIdCek));
                memset(prevInputIdCek, 0, sizeof(prevInputIdCek));
                warningMsg = "";
            }
            ImGui::Spacing();
            ImGui::SetCursorPosX((cardW - 350) / 2);
            ImGui::TextDisabled("----------------------------------------");
            if (centeredBtn("Keluar Aplikasi", 350, 36))
                glfwSetWindowShouldClose(window, true);

            ImGui::EndChild();
        }

        // ============================================================
        //  PILIH RUTE
        // ============================================================
        else if (appState == STATE_PILIH_RUTE) {
            RenderProgressSteps(1);
            if (ImGui::Button("<< Kembali ke Beranda")) appState = STATE_HOME;
            ImGui::Spacing();
            ImGui::Text("Pilih jadwal perjalanan Anda:");
            ImGui::Spacing();

            const char* tujuan[] = { "Jakarta","Jogja","Bali" };
            int         startIdx[] = { 0,3,6 };

            for (int t = 0;t < 3;t++) {
                bool open = ImGui::CollapsingHeader(
                    (string("  Malang  >>  ") + tujuan[t]).c_str(),
                    ImGuiTreeNodeFlags_DefaultOpen);
                if (!open) continue;

                for (int i = startIdx[t]; i < startIdx[t] + 3; i++) {
                    Bus& b = daftarBus[i];
                    int  avail = HitungKursiTersedia(i);

                    ImVec4 bg = (b.kelas == "Ekonomi") ? ImVec4(0.10f, 0.28f, 0.13f, 0.8f) :
                        (b.kelas == "Eksekutif") ? ImVec4(0.10f, 0.23f, 0.48f, 0.8f) :
                        ImVec4(0.33f, 0.13f, 0.48f, 0.8f);

                    ImGui::PushStyleColor(ImGuiCol_ChildBg, bg);
                    string cardId = "##card" + to_string(i);
                    ImGui::BeginChild(cardId.c_str(), ImVec2(0, 78), true,
                        ImGuiWindowFlags_NoScrollbar);

                    ImGui::Text("  Berangkat: %s   Kelas: %-10s   Kursi tersedia: %d/%d",
                        b.jamBerangkat.c_str(), b.kelas.c_str(), avail, b.totalKursi);
                    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.3f, 1),
                        "  Fasilitas: %s", b.fasilitas.c_str());

                    float priceX = ImGui::GetContentRegionAvail().x - 200;
                    ImGui::SameLine(priceX);
                    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1),
                        "%s", FormatRupiah(b.harga).c_str());
                    ImGui::SameLine();

                    if (avail == 0) {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.4f, 0.4f, 1));
                        ImGui::Button("HABIS", ImVec2(80, 28));
                        ImGui::PopStyleColor();
                    }
                    else {
                        string btnLabel = "Pilih##" + to_string(i);
                        if (ImGui::Button(btnLabel.c_str(), ImVec2(80, 28))) {
                            busTerpilih = i;
                            kursiTerpilih.clear();
                            memset(inputNama, 0, sizeof(inputNama));
                            warningMsg = "";
                            appState = STATE_PILIH_KURSI;
                        }
                    }
                    ImGui::EndChild();
                    ImGui::PopStyleColor();
                    ImGui::Spacing();
                }
            }
        }

        // ============================================================
        //  PILIH KURSI  (multi-select)
        // ============================================================
        else if (appState == STATE_PILIH_KURSI) {
            RenderProgressSteps(2);
            Bus& b = daftarBus[busTerpilih];
            int  totalBayar = b.harga * (int)kursiTerpilih.size();

            if (ImGui::Button("<< Ganti Jadwal")) {
                appState = STATE_PILIH_RUTE;
                kursiTerpilih.clear();
            }
            ImGui::Spacing();

            // Info perjalanan
            ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1),
                "Malang >> %s  |  Kelas: %s  |  %s  |  %s / kursi",
                b.rute.c_str(), b.kelas.c_str(),
                b.jamBerangkat.c_str(),
                FormatRupiah(b.harga).c_str());
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.3f, 1),
                "Fasilitas: %s", b.fasilitas.c_str());
            ImGui::Spacing();

            // Input nama
            ImGui::Text("Nama lengkap Anda:"); ImGui::SameLine();
            ImGui::SetNextItemWidth(300);
            ImGui::InputText("##nama", inputNama, IM_ARRAYSIZE(inputNama));
            ImGui::Spacing();

            // Pesan error/info
            if (!warningMsg.empty())
                ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "[!] %s", warningMsg.c_str());
            ImGui::Spacing();

            // Legenda
            ImGui::TextDisabled("Legenda: ");   ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.5f, 0.1f, 1));
            ImGui::SmallButton("  Tersedia  "); ImGui::PopStyleColor(); ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.65f, 0.0f, 1));
            ImGui::SmallButton("  Dipilih   "); ImGui::PopStyleColor(); ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65f, 0.1f, 0.1f, 1));
            ImGui::SmallButton("  Terisi    "); ImGui::PopStyleColor();
            ImGui::Spacing();
            ImGui::TextDisabled("<-- Kiri/Jendela                           Kanan/Jendela -->");
            ImGui::Separator(); ImGui::Spacing();

            // Denah kursi
            ImGui::BeginChild("##SeatMap", ImVec2(0, 360), false);
            for (int i = 0;i < b.jumlahBaris;i++) {
                ImGui::TextDisabled("Baris %2d  ", i + 1); ImGui::SameLine();
                for (int j = 0;j < b.jumlahKolom;j++) {
                    int  no = i * b.jumlahKolom + j + 1;
                    char buf[8]; sprintf(buf, " %02d ", no);
                    bool dipilih = KursiSudahDipilih(i, j);

                    if (b.kursi[i][j].status == 1) {
                        // Terisi – tidak bisa diklik
                        ImGui::PushStyleColor(ImGuiCol_Button,
                            ImVec4(0.65f, 0.1f, 0.1f, 1));
                        ImGui::Button("####", ImVec2(50, 44));
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("Kursi sudah dipesan");
                    }
                    else if (dipilih) {
                        // Dipilih customer – klik lagi untuk batal pilih
                        ImGui::PushStyleColor(ImGuiCol_Button,
                            ImVec4(0.7f, 0.65f, 0.0f, 1));
                        if (ImGui::Button((string("[  ") + to_string(no) + "  ]").c_str(),
                            ImVec2(50, 44)))
                            ToggleKursi(i, j);   // deselect
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("Klik lagi untuk batal pilih");
                    }
                    else {
                        // Tersedia
                        ImGui::PushStyleColor(ImGuiCol_Button,
                            ImVec4(0.1f, 0.5f, 0.1f, 1));
                        if (ImGui::Button(buf, ImVec2(50, 44)))
                            ToggleKursi(i, j);   // select
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("Klik untuk memilih");
                    }
                    ImGui::PopStyleColor();
                    if (j < b.jumlahKolom - 1) ImGui::SameLine();
                    if (j == b.formatKiri - 1) { ImGui::SameLine(); ImGui::Dummy(ImVec2(18, 0)); ImGui::SameLine(); }
                }
            }
            ImGui::EndChild();

            ImGui::Separator(); ImGui::Spacing();

            // Ringkasan bawah
            if (!kursiTerpilih.empty()) {
                // Tampilkan nomor kursi yang dipilih
                string listNo = "";
                for (int k = 0;k < (int)kursiTerpilih.size();k++) {
                    if (k > 0) listNo += ", ";
                    listNo += to_string(
                        kursiTerpilih[k].row * b.jumlahKolom + kursiTerpilih[k].col + 1);
                }
                ImGui::TextColored(ImVec4(1, 1, 0, 1),
                    "Kursi dipilih: %s   (%d kursi)   Total: %s",
                    listNo.c_str(), (int)kursiTerpilih.size(),
                    FormatRupiah(totalBayar).c_str());
                ImGui::Spacing();

                if (ImGui::Button("Lanjut ke Konfirmasi  >>", ImVec2(270, 44))) {
                    if (strlen(inputNama) < 4) {
                        warningMsg = "Mohon isi nama lengkap Anda terlebih dahulu (min. 4 huruf).";
                    }
                    else {
                        warningMsg = "";
                        appState = STATE_KONFIRMASI;
                    }
                }
            }
            else {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1),
                    "Silakan pilih kursi yang Anda inginkan (bisa lebih dari satu).");
            }
        }

        // ============================================================
        //  KONFIRMASI
        // ============================================================
        else if (appState == STATE_KONFIRMASI) {
            RenderProgressSteps(3);
            Bus& b = daftarBus[busTerpilih];
            int  totalBayar = b.harga * (int)kursiTerpilih.size();

            if (ImGui::Button("<< Ubah Pilihan Kursi")) appState = STATE_PILIH_KURSI;
            ImGui::Spacing();

            ImGui::BeginChild("##konfirmasi", ImVec2(500, 300), true);
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.0f, 1), "RINGKASAN PEMESANAN");
            ImGui::Separator(); ImGui::Spacing();

            auto Row = [](const char* lbl, const string& val) {
                ImGui::TextDisabled("  %-22s", lbl);
                ImGui::SameLine(175); ImGui::Text(": %s", val.c_str());
                };

            // Daftar nomor kursi
            string listNo = "";
            for (int k = 0;k < (int)kursiTerpilih.size();k++) {
                if (k > 0) listNo += ", ";
                listNo += to_string(
                    kursiTerpilih[k].row * b.jumlahKolom + kursiTerpilih[k].col + 1);
            }

            Row("Nama Penumpang", inputNama);
            Row("Rute", "Malang >> " + b.rute);
            Row("Kelas Bus", b.kelas);
            Row("Jam Berangkat", b.jamBerangkat);
            Row("Nomor Kursi", listNo + " (" + to_string(kursiTerpilih.size()) + " kursi)");
            Row("Fasilitas", b.fasilitas);
            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1),
                "  Total Pembayaran  : %s  (%d x %s)",
                FormatRupiah(totalBayar).c_str(),
                (int)kursiTerpilih.size(),
                FormatRupiah(b.harga).c_str());
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1),
                "  [!] Pastikan data sudah benar sebelum melanjutkan.");
            ImGui::Spacing();
            ImGui::EndChild();
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Button,
                ImVec4(0.1f, 0.55f, 0.2f, 1));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                ImVec4(0.2f, 0.75f, 0.3f, 1));
            if (ImGui::Button("[OK]  Konfirmasi & Selesaikan Pemesanan",
                ImVec2(370, 50))) {
                // Proses semua kursi sekaligus
                lastBookingIds.clear();
                for (auto& k : kursiTerpilih) {
                    string newId = b.kodePrefix + "-" + to_string(idCounter++);
                    b.kursi[k.row][k.col].status = 1;
                    b.kursi[k.row][k.col].nama = inputNama;
                    b.kursi[k.row][k.col].idBooking = newId;
                    lastBookingIds.push_back(newId);
                }
                appState = STATE_SUKSES;
            }
            ImGui::PopStyleColor(2);
        }

        // ============================================================
        //  SUKSES
        // ============================================================
        else if (appState == STATE_SUKSES) {
            Bus& b = daftarBus[busTerpilih];
            int  totalBayar = b.harga * (int)lastBookingIds.size();

            float cardW = 520;
            ImGui::SetCursorPosX(((float)W - cardW) / 2);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.05f, 0.18f, 0.09f, 1));
            ImGui::BeginChild("##sukses", ImVec2(cardW, 360), true);

            ImGui::Spacing();
            ImGui::SetCursorPosX(20);
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1),
                "*** PEMESANAN BERHASIL! ***");
            ImGui::Separator(); ImGui::Spacing();
            ImGui::Text("  Halo, %s! Tiket Anda telah berhasil dipesan.", inputNama);
            ImGui::Spacing();

            // Kotak ID booking
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.07f, 0.07f, 0.07f, 1));
            ImGui::BeginChild("##tiket_mini", ImVec2(cardW - 30, 180), true);
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.0f, 1),
                "  ID BOOKING ANDA (simpan baik-baik!):");
            ImGui::Spacing();

            // Tampilkan setiap ID + nomor kursi
            for (int k = 0;k < (int)lastBookingIds.size();k++) {
                int noKursi = kursiTerpilih[k].row * b.jumlahKolom
                    + kursiTerpilih[k].col + 1;
                ImGui::TextColored(ImVec4(1, 1, 0, 1),
                    "  Kursi %02d  >>  %s",
                    noKursi, lastBookingIds[k].c_str());
            }
            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
            ImGui::Text("  Rute  : Malang >> %s (%s)   Jam: %s",
                b.rute.c_str(), b.kelas.c_str(), b.jamBerangkat.c_str());
            ImGui::Text("  Total : %s  (%d kursi)",
                FormatRupiah(totalBayar).c_str(), (int)lastBookingIds.size());
            ImGui::Spacing();
            ImGui::EndChild();
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.8f, 0.7f, 0.2f, 1),
                "  [!] Gunakan ID Booking di atas untuk cek atau batalkan tiket.");
            ImGui::Spacing();

            if (ImGui::Button("  Kembali ke Beranda  ", ImVec2(-1, 40))) {
                appState = STATE_HOME;
                memset(inputNama, 0, sizeof(inputNama));
                kursiTerpilih.clear();
                lastBookingIds.clear();
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }

        // ============================================================
        //  CEK TIKET
        // ============================================================
        else if (appState == STATE_CEK_TIKET) {
            if (ImGui::Button("<< Kembali ke Beranda")) {
                appState = STATE_HOME;
                warningMsg = "";
                memset(inputIdCek, 0, sizeof(inputIdCek));
                memset(prevInputIdCek, 0, sizeof(prevInputIdCek));
            }
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.0f, 1), "CEK / BATALKAN TIKET");
            ImGui::Separator(); ImGui::Spacing();

            ImGui::Text("Masukkan ID Booking Anda:"); ImGui::SameLine();
            ImGui::SetNextItemWidth(280);
            ImGui::InputText("##idcek", inputIdCek, IM_ARRAYSIZE(inputIdCek));
            ImGui::TextDisabled("  Contoh: JKT-EKO-1001");
            ImGui::Spacing();

            // FIX: reset warningMsg jika input berubah
            if (strcmp(inputIdCek, prevInputIdCek) != 0) {
                warningMsg = "";
                strcpy(prevInputIdCek, inputIdCek);
            }

            if (strlen(inputIdCek) > 0) {
                SearchResult res = FindBookingById(inputIdCek);
                if (res.found) {
                    Bus& b = daftarBus[res.busIdx];
                    DataKursi& k = b.kursi[res.row][res.col];
                    int noK = res.row * b.jumlahKolom + res.col + 1;

                    ImGui::BeginChild("##detail", ImVec2(480, 200), true);
                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.4f, 1),
                        "  [OK] Tiket Ditemukan");
                    ImGui::Separator(); ImGui::Spacing();

                    auto Row2 = [](const char* lbl, const string& v) {
                        ImGui::TextDisabled("  %-18s", lbl);
                        ImGui::SameLine(155); ImGui::Text(": %s", v.c_str());
                        };
                    Row2("Nama", k.nama);
                    Row2("Rute", "Malang >> " + b.rute);
                    Row2("Kelas", b.kelas);
                    Row2("Jam Berangkat", b.jamBerangkat);
                    Row2("Nomor Kursi", to_string(noK));
                    Row2("Harga Tiket", FormatRupiah(b.harga));
                    ImGui::Spacing();
                    ImGui::EndChild();

                    // Tampilkan pesan sukses batal (setelah tiket dibatalkan)
                    if (!warningMsg.empty()) {
                        ImGui::Spacing();
                        ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.4f, 1),
                            "  [OK] %s", warningMsg.c_str());
                    }
                    else {
                        // Hanya tampilkan tombol batalkan jika belum dibatalkan
                        ImGui::Spacing();
                        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1),
                            "[!] Pembatalan bersifat permanen dan tidak dapat diurungkan.");
                        ImGui::Spacing();
                        ImGui::PushStyleColor(ImGuiCol_Button,
                            ImVec4(0.65f, 0.1f, 0.1f, 1));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                            ImVec4(0.9f, 0.2f, 0.2f, 1));
                        if (ImGui::Button("Batalkan Tiket Ini", ImVec2(220, 44))) {
                            k = { 0,"","" };
                            warningMsg = "Tiket berhasil dibatalkan.";
                            // Kosongkan inputIdCek agar tidak menampilkan data lama
                            memset(inputIdCek, 0, sizeof(inputIdCek));
                            memset(prevInputIdCek, 0, sizeof(prevInputIdCek));
                        }
                        ImGui::PopStyleColor(2);
                    }

                }
                else {
                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1),
                        "[!] ID Booking tidak ditemukan.");
                    ImGui::TextDisabled(
                        "    Pastikan penulisan benar (huruf kapital, contoh: JKT-EKO-1001)");
                }
            }

            // Tampilkan warningMsg jika inputIdCek kosong (setelah batal)
            if (strlen(inputIdCek) == 0 && !warningMsg.empty()) {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.4f, 1),
                    "  [OK] %s", warningMsg.c_str());
            }
        }

        ImGui::End();

        ImGui::Render();
        glViewport(0, 0, W, H);
        glClearColor(0.08f, 0.10f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}