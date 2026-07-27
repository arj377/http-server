console.log("JavaScript loaded successfully!");

document.addEventListener("DOMContentLoaded", () => {
    console.log("Page fully loaded.");

    const heading = document.querySelector("h1");
    heading.addEventListener("click", () => {
        alert("Your custom HTTP server is serving JavaScript correctly!");
    });
});