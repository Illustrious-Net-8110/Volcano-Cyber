const UnsplashBackground = {
  accessKey: "YOUR_UNSPLASH_API_KEY", // You'll need a free API key from https://unsplash.com/developers 
  query: "cannabis,city,night",
  fallbackImage: "/images/fallback.jpg",
  refreshInterval: 300000, // 5 Minutes
  targetClass: "main",

  async update() {
    try {
      const url = `https://api.unsplash.com/photos/random?query=${this.query}&client_id=${this.accessKey}`;
      const res = await fetch(url);

      if (!res.ok) throw new Error("Unsplash API error");

      const data = await res.json();
      const imageUrl = data.urls.regular;

      this.set(imageUrl);
      console.log("Background updated:", imageUrl);
    } catch (err) {
      console.warn("Using fallback image due to error:", err);
      this.set(this.fallbackImage);
    }
  },

  set(url) {
    const el = document.querySelector(`.${this.targetClass}`);
    if (el) {
      el.style.backgroundImage = `url('${url}')`;
      el.style.backgroundSize = "cover";
      el.style.backgroundPosition = "center";
      el.style.backgroundRepeat = "no-repeat";
    }
  },

  startAutoUpdate() {
    this.update();
    setInterval(() => this.update(), this.refreshInterval);
  }
};

document.addEventListener("DOMContentLoaded", () => {
  UnsplashBackground.startAutoUpdate();
});
