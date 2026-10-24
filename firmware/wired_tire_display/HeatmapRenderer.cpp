#include "HeatmapRenderer.h"
void HeatmapRenderer::poll(uint64_t now,const std::array<wired_tire::SnapshotDelivery,4>&deliveries,const wired_tire::ColorScale&scale,PanelDriver&panel,wired_brake::TelemetryUi&ui){
  if(now<nextDrawUs_)return;
  auto*buffer=panel.drawBuffer();if(!buffer)return;
  wired_tire::PixelSurface surface{buffer,1024,600,1024,1024*600};ui.render(deliveries,now,scale,surface);
  if(panel.present(buffer,surface.capacity)){++submitted_;nextDrawUs_=wired_brake::deadlineAfter(now,33334);}
}
void HeatmapRenderer::poll(uint64_t nowUs, const std::array<wired_tire::SnapshotDelivery,4>& deliveries,
    const wired_tire::ColorScale& scale, PanelDriver& panel) {
  if (nowUs < nextDrawUs_) return;
  auto* buffer = panel.drawBuffer();
  if (!buffer) return;
  wired_tire::PixelSurface surface{buffer,1024,600,1024,1024 * 600};
  wired_tire::renderDashboard(deliveries, nowUs, scale, surface);
  if (panel.present(buffer, surface.capacity)) {
    ++submitted_;
    nextDrawUs_ = nowUs + 33334; // Target limit only; never report this as measured panel FPS.
  }
}
