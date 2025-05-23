# Kúpözön
Dülöngélő hengerek és kúpok inkrementális képszintézissel

## Leírás
Írjon programot a 4. feladat specifikációját megcélozva, a következő változtatásokkal:
- A megjelenítő algoritmus most inkrementális képszintézis.
- A feladatban szereplő objektumokat háromszögekre kell tesszellálni. Egy-egy kúpot és hengert 12 háromszöggel kell közelíteni, a tengely irányában csak a két végén vannak csúcspontok, arra merőleges irányban viszont mindkét végen 6. Az alapnégyzetet két háromszögre kell bontani és sakktábla textúrával ellátni úgy, hogy a kép hasonló legyen a sugárkövetési eredményhez.
- Az optikailag sima anyagokat (arany és víz) most rücskös anyagokkal kell kiváltani, mégpedig úgy, hogy az n törésmutató diffúz visszaverődési tényezővé, a kappa kioltási tényező pedig spekuláris visszaverődési tényezővé válik. 
- Az irányfényforrás árnyékát a pixel shaderben megvalósított sugárkövetéses algoritmussal kell kiszámítani. Az árnyékban csak az ambiens megvilágítás érvényesül. Az árnyékok meghatározását végző módszer ugyancsak a háromszögek takarását ellenőrzi, amit uniform változókban kell átadni a pixel shadernek.
