# Homebrew store preparation

## Nintendo 3DS — Universal-Updater / Universal-DB

The repository is structured for GitHub Releases, the distribution method
recommended by Universal-DB. Each tagged release contains
`EllisHouse-3DSX.zip` and `EllisHouse-3DS.cia` with stable names.

The submission record is in `packaging/universal-db/ellis-house.json`. Before
submitting it to Universal-Team/db, verify the final GitHub owner/repository.
The record declares AI-assisted port development as required by that database.

## PSP stores

PSP stores do not share one universal manifest. The release therefore provides
a conventional `PSP/GAME/EllisHousePSP` ZIP, neutral metadata and a 250×320 PNG
cover under `packaging/psp-store/`.

- Download: `EllisHouse-PSP.zip`
- Install destination: `/PSP/GAME/EllisHousePSP`
- Entry point: `/PSP/GAME/EllisHousePSP/EBOOT.PBP`
- Cover: `artwork/psp/retroflow/EllisHousePSP.png`
- Homepage: `https://glacial-moon-games.itch.io/ellishouse`

Before requesting store inclusion, test the release on target hardware and
verify its hashes against `SHA256SUMS.txt`.
